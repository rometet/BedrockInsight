#include "bedrock_insight/levilamina/player_hurt.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/hurt_observation.hpp"
#include "bedrock_insight/core/item_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
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
// Pre-origin snapshot. The strict break proof re-reads the inventory slot after
// origin; the upstream `used` counter only needs the item's own durability
// classification, which must be copied before the stack can be destroyed.
struct HurtEntry {
    WeakEntityRef weak;
    Player const* player_identity;
    Inventory const* inventory;
    ItemStackBase const* item_identity;
    int slots,slot;
    short max_damage;
    short damage_before;
    AnalyticsEvent value;
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
            if(player && static_cast<::Actor*>(player)==owner) {
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
                            static_cast<std::int8_t>(static_cast<int>(player->getDimensionId()))};
                    }
                    // Strict break snapshot: only a single-unit damageable item
                    // resolved to a slot of the same main inventory can be proven.
                    if(mCount==1 && damageable && max_damage>1 && damage_before>=0
                        && damage_before<=max_damage
                        && static_cast<long long>(damage_before)+deltaDamage>=static_cast<long long>(max_damage)) {
                        auto const& inventory=player->getInventory();
                        auto slots=inventory.getContainerSize();
                        if(slots>=1 && slots<=128) {
                            int slot=-1;
                            for(int i=0;i<slots;++i) {
                                if(static_cast<ItemStackBase const*>(&inventory.getItem(i))==this){slot=i;break;}
                            }
                            if(slot>=0) {
                                HurtEntry entry{player->getEntityContext().getWeakRef(),player,&inventory,this,
                                    slots,slot,static_cast<short>(max_damage),
                                    static_cast<short>(damage_before),{}};
                                entry.value.kind=EventKind::ItemBrokenResult;
                                entry.value.actor.kind=ActorKind::Player;
                                entry.value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
                                entry.value.actor.display_name=player->getRealName();
                                if(entry.value.actor.display_name.size()>256)entry.value.actor.display_name.resize(256);
                                entry.value.primary_type=getTypeName();
                                entry.value.cause="ll26.60.durability_item_destroyed.v1";
                                broken=std::move(entry);
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
        if(!player || player.as_ptr()!=broken->player_identity)return accepted;
        auto const& inventory=player->getInventory();
        if(&inventory!=broken->inventory)return accepted;
        auto slots=inventory.getContainerSize();
        if(slots!=broken->slots)return accepted;
        auto const& after=inventory.getItem(broken->slot);
        if(!hurt_slot_became_empty(deltaDamage,1,after.mCount,broken->slots,slots,broken->slot,
            true,static_cast<ItemStackBase const*>(&after)==broken->item_identity))return accepted;
        auto& entry=*broken;
        entry.value.amount=1;
        entry.value.occurred_at_ms=now_ms();
        entry.value.position={0,0,0,static_cast<std::int8_t>(static_cast<int>(player->getDimensionId()))};
        entry.value.details_json="{\"hook\":\"ItemStackBase::hurtAndBreak\",\"phase\":\"return_true_item_destroyed\",\"scope\":\"main_inventory_same_slot\",\"delta_damage\":"
            +std::to_string(deltaDamage)+",\"slots\":"+std::to_string(slots)+",\"slot\":"+std::to_string(entry.slot)
            +",\"max_damage\":"+std::to_string(entry.max_damage)
            +",\"damage_before\":"+std::to_string(entry.damage_before)
            +",\"is_damageable\":true,\"entry_units\":1,\"after_units\":0,\"nested\":false,"
            "\"durability_path\":true,\"durability_exhausted_verified\":true,\"max_damage_value_exceeded_read\":true}";
        state->sink(std::move(entry.value));
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
