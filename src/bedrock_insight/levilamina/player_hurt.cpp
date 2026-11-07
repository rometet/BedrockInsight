#include "bedrock_insight/levilamina/player_hurt.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/hurt_observation.hpp"
#include "bedrock_insight/core/broken_record.hpp"
#include "bedrock_insight/core/item_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

namespace bedrock_insight::levilamina {
class HurtCapture final {
public:
    explicit HurtCapture(HurtSink fn):sink(std::move(fn)){}
    HurtSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
HurtToken current;
bool installed=false,install_failed=false;
thread_local bool inside_hurt=false,nested_hurt=false;
HurtToken current_capture() noexcept {
    try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}
}
struct HurtScope final {
    HurtScope(){inside_hurt=true;nested_hurt=false;}
    ~HurtScope(){inside_hurt=false;nested_hurt=false;}
};
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
// Copy before origin; never dereference this stack or the owner pointer after
// origin. Equipment accessors are the pinned public LL functions, not offsets.
struct HurtEntry {
    WeakEntityRef weak;
    Player const* player_identity;
    Inventory const* inventory;
    ItemStackBase const* item_identity;
    Level const* level;
    BrokenObservation observation;
    Actor actor;
    std::string type;
};
struct UsedEntry {
    WeakEntityRef weak;
    Player const* player_identity;
    std::string type;
    bool damageable=false;
    bool armor=false;
    short max_damage=0;
    short damage_before=0;
    std::int8_t dimension=0;
    Level const* level=nullptr;
};
LL_TYPE_INSTANCE_HOOK(HurtSlotHook,ll::memory::HookPriority::Lowest,ItemStackBase,
    &ItemStackBase::hurtAndBreak,bool,int deltaDamage,::Actor* owner) {
    if(inside_hurt){nested_hurt=true;return origin(deltaDamage,owner);}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire))return origin(deltaDamage,owner);
    HurtScope scope;
    std::optional<HurtEntry> broken;
    std::optional<UsedEntry> used;
    // --- pre-origin snapshots ------------------------------------------------
    try {
        if(owner && deltaDamage>0) {
            auto* player=Player::tryGetFromEntity(owner->getEntityContext(),false);
            if(player && static_cast<::Actor*>(player)==owner && !player->mRemoved
                && static_cast<int>(player->getDimensionId())>=-1 && static_cast<int>(player->getDimensionId())<=127) {
                auto const& uuid=player->getUuid();
                if(uuid.a!=0 || uuid.b!=0) {
                    auto type=getTypeName();
                    auto* item=mItem.get();
                    bool const damageable=item!=nullptr && item->isDamageable();
                    bool const armor=damageable && isArmorItem();
                    auto const max_damage=damageable ? item->getMaxDamage() : 0;
                    auto const damage_before=getDamageValue();
                    if(valid_item_type(type)) {
                        used=UsedEntry{player->getEntityContext().getWeakRef(),player,std::move(type),
                            damageable,armor,static_cast<short>(max_damage),
                            static_cast<short>(damage_before),
                            static_cast<std::int8_t>(static_cast<int>(player->getDimensionId())),&player->getLevel()};
                    }
                    // Only one exact owned stack location may match. Copies,
                    // ambiguous aliases, invalid counts and non-threshold calls
                    // are not converted into a broken-item observation.
                    if(mCount==1 && damageable && max_damage>1 && damage_before>=0
                        && damage_before<=max_damage
                        && static_cast<long long>(damage_before)+deltaDamage>=static_cast<long long>(max_damage)) {
                        auto const& inventory=player->getInventory();
                        auto slots=inventory.getContainerSize();
                        if(slots>=1 && slots<=128) {
                            BrokenObservation observed;int matches=0;
                            auto match=[&](ItemStack const& stack,BrokenLocation location,int slot,int count){
                                if(static_cast<ItemStackBase const*>(&stack)==this){
                                    ++matches;observed={location,count,slot,max_damage,damage_before,deltaDamage,1,0,
                                        static_cast<int>(player->getDimensionId())};
                                }
                            };
                            for(int i=0;i<slots;++i) {
                                match(inventory.getItem(i),BrokenLocation::Inventory,i,slots);
                            }
                            match(player->getOffhandSlot(),BrokenLocation::Offhand,0,1);
                            for(int i=0;i<4;++i)match(player->getArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(i)),BrokenLocation::Armor,i,4);
                            if(matches==1 && broken_slot_valid(observed)) {
                                Actor actor{ActorKind::Player,"uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b),player->getRealName()};
                                if(actor.display_name.size()>256)actor.display_name.resize(256);
                                broken=HurtEntry{player->getEntityContext().getWeakRef(),player,&inventory,this,
                                    &player->getLevel(),observed,std::move(actor),getTypeName()};
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {}
    bool const accepted=origin(deltaDamage,owner);
    if(!accepted || nested_hurt || !state->active.load(std::memory_order_acquire))return accepted;
    // --- upstream `used`: non-armor items that consumed durability ----------
    if(used) {
        try {
            auto player=used->weak.tryUnwrap<Player>();
            if(player && player.as_ptr()==used->player_identity
                && !player->mRemoved && &player->getLevel()==used->level
                && static_cast<int>(player->getDimensionId())==used->dimension
                && item_hurt_counts_as_used(true,used->damageable,used->armor,deltaDamage)
                && state->active.load(std::memory_order_acquire)) {
                AnalyticsEvent value;
                value.kind=EventKind::ItemUseResult;
                value.actor.kind=ActorKind::Player;
                value.actor.id="uuid64:"+std::to_string(player->getUuid().a)+":"
                    +std::to_string(player->getUuid().b);
                value.actor.display_name=player->getRealName();
                if(value.actor.display_name.size()>256)value.actor.display_name.resize(256);
                value.primary_type=used->type;
                value.secondary_type="durability";
                value.amount=1;
                value.occurred_at_ms=now_ms();
                value.position={0,0,0,used->dimension};
                value.cause="ll26.60.item_hurt_used.v1";
                value.details_json="{\"hook\":\"ItemStackBase::hurtAndBreak\",\"phase\":\"post_origin_true\","
                    "\"item_type\":\""+used->type+"\",\"delta_damage\":"+std::to_string(deltaDamage)
                    +",\"max_damage\":"+std::to_string(used->max_damage)
                    +",\"damage_before\":"+std::to_string(used->damage_before)
                    +",\"armor\":false,\"nested\":false}";
                state->sink(std::move(value));
            }
        } catch (...) {}
    }
    // --- strict destroyed-item proof ---------------------------------------
    if(!broken || !state->active.load(std::memory_order_acquire))return accepted;
    try {
        auto player=broken->weak.tryUnwrap<Player>();
        if(!player || player.as_ptr()!=broken->player_identity || player->mRemoved)return accepted;
        if(!broken_frame_valid(false,true,&player->getLevel()==broken->level,broken->observation.dimension,
            static_cast<int>(player->getDimensionId())))return accepted;
        auto observed=broken->observation;ItemStack const* after=nullptr;
        switch(observed.location) {
        case BrokenLocation::Inventory:{auto const& inventory=player->getInventory();
            if(&inventory!=broken->inventory || inventory.getContainerSize()!=observed.slots)return accepted;
            after=&inventory.getItem(observed.slot);break;}
        case BrokenLocation::Offhand:after=&player->getOffhandSlot();break;
        case BrokenLocation::Armor:after=&player->getArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(observed.slot));break;
        }
        if(!after || static_cast<ItemStackBase const*>(after)!=broken->item_identity)return accepted;
        observed.after_units=after->mCount;
        auto value=broken_record(broken->actor,broken->type,observed,now_ms());
        if(value && state->active.load(std::memory_order_acquire))state->sink(std::move(*value));
    } catch (...) {}
    return accepted;
}
}
HurtToken start_hurt_capture(HurtSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if(install_failed || current_capture())return {};
        if(!installed){if(HurtSlotHook::hook()!=0){install_failed=true;return {};}installed=true;}
        auto token=std::make_shared<HurtCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_hurt_capture(HurtToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
}
