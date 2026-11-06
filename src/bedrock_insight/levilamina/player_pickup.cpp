#include "bedrock_insight/levilamina/player_pickup.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/pickup_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/item/ItemActor.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class PickupCapture final {
public:
    explicit PickupCapture(PickupSink fn):sink(std::move(fn)){}
    PickupSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
PickupToken current;
bool installed=false,install_failed=false;
thread_local bool inside_pickup=false,nested_pickup=false;
PickupToken current_capture() noexcept {
    try {std::lock_guard lock(current_mutex);return current;}catch(...){return {};}
}
struct PickupScope final {
    PickupScope(){inside_pickup=true;nested_pickup=false;}
    ~PickupScope(){inside_pickup=false;nested_pickup=false;}
};
struct InventoryCount {int slots;std::int64_t units;};
std::optional<InventoryCount> count_type(Inventory const& inventory,std::string const& type) {
    auto const size=inventory.getContainerSize();
    if(size<1 || size>128)return {};
    std::int64_t count=0;
    for(int i=0;i<size;++i) {
        auto const& item=inventory.getItem(i); // official const virtual API, not MCNAPI thunk/slot guess
        if(item.mCount && item.getTypeName()==type)count+=item.mCount;
    }
    return InventoryCount{size,count};
}
struct PickupEntry {
    WeakEntityRef weak;
    Player const* identity;
    Inventory const* inventory;
    InventoryCount before;
    int entry_units;
    AnalyticsEvent value;
};
LL_TYPE_INSTANCE_HOOK(PickupDeltaHook,ll::memory::HookPriority::Lowest,Player,
    &Player::take,bool,::Actor& actor,int orgCount,int favoredSlot) {
    if(inside_pickup){nested_pickup=true;return origin(actor,orgCount,favoredSlot);}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire))return origin(actor,orgCount,favoredSlot);
    PickupScope scope;
    return observe_pickup_result([&]() -> std::optional<PickupEntry> {
        if(!state->active.load(std::memory_order_acquire))return {};
        // Checked official LL conversion, never a category-only unchecked cast.
        auto* item_actor=ItemActor::tryGetFromEntity(actor.getEntityContext(),false);
        if(!item_actor || static_cast<::Actor*>(item_actor)!=&actor)return {};
        auto const& uuid=getUuid();if(uuid.a==0 && uuid.b==0)return {};
        auto const& item=static_cast<ItemActor const&>(*item_actor).item();
        auto type=item.getTypeName();
        if(!item.mCount || !valid_item_type(type))return {};
        auto const& inventory=getInventory();
        auto before=count_type(inventory,type);if(!before)return {};
        PickupEntry entry{getEntityContext().getWeakRef(),this,&inventory,*before,item.mCount,{}};
        entry.value.kind=EventKind::ItemPickupDelta;
        entry.value.actor.kind=ActorKind::Player;
        entry.value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        entry.value.actor.display_name=getRealName();
        if(entry.value.actor.display_name.size()>256)entry.value.actor.display_name.resize(256);
        entry.value.primary_type=std::move(type);
        entry.value.cause="ll26.60.take_inventory_delta.v1";
        return entry;
    },[&]{return origin(actor,orgCount,favoredSlot);},[&](PickupEntry& entry) {
        if(nested_pickup || !state->active.load(std::memory_order_acquire))return false;
        // No original Player/ItemActor/ItemStack is read after origin.
        auto player=entry.weak.tryUnwrap<Player>();
        if(!player || player.as_ptr()!=entry.identity)return false;
        auto const& inventory=player->getInventory();
        if(&inventory!=entry.inventory)return false;
        auto after=count_type(inventory,entry.value.primary_type);if(!after)return false;
        auto gain=pickup_inventory_gain(entry.entry_units,entry.before.slots,after->slots,
            entry.before.units,after->units,true);
        if(!gain)return false;
        entry.value.amount=*gain;
        entry.value.details_json="{\"hook\":\"Player::take\",\"phase\":\"return_true_inventory_delta\",\"scope\":\"main_inventory_type_total\",\"entry_units\":"
            +std::to_string(entry.entry_units)+",\"slots\":"+std::to_string(after->slots)+",\"before_units\":"
            +std::to_string(entry.before.units)+",\"after_units\":"+std::to_string(after->units)
            +",\"nested\":false,\"durable_transfer_verified\":false}";
        return true;
    },[&](PickupEntry entry) {
        if(!state->active.load(std::memory_order_acquire))return;
        entry.value.occurred_at_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state->sink(std::move(entry.value));
    });
}
}
PickupToken start_pickup_capture(PickupSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if(install_failed || current_capture())return {};
        if(!installed){if(PickupDeltaHook::hook()!=0){install_failed=true;return {};}installed=true;}
        auto token=std::make_shared<PickupCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}
        return token;
    }catch(...){return {};}
}
void stop_pickup_capture(PickupToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try {std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
