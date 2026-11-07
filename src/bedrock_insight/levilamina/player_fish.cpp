#include "bedrock_insight/levilamina/player_fish.hpp"
#include "bedrock_insight/core/fish_bucket.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/item/BucketItem.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class FishCapture {
public:
    explicit FishCapture(FishSink fn):sink(std::move(fn)){}
    FishSink sink;std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
FishToken current;bool installed=false,failed=false;
FishToken capture() noexcept {try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}}
struct Context {
    ::Actor const* target=nullptr;std::int64_t uid=0;
    FishBucketOutcome outcome;
};
thread_local Context* context=nullptr;
thread_local unsigned depth=0;
struct CallScope {
    Context* saved;
    explicit CallScope(Context* next):saved(context){context=next;++depth;}
    ~CallScope(){--depth;context=saved;}
};
struct Count {int slots=0;std::int64_t units=0;};
std::optional<Count> count(Inventory const& inventory,std::string_view type) {
    int const slots=inventory.getContainerSize();if(slots<1 || slots>128)return {};
    Count result{slots,0};
    for(int i=0;i<slots;++i){auto const& item=inventory.getItem(i);if(item.mCount && item.getTypeName()==type)result.units+=item.mCount;}
    return result;
}
struct Entry {
    WeakEntityRef player,target;
    Player const* identity;::Actor const* target_identity;Inventory const* inventory;
    Level const* level;int dimension;Actor actor;std::string type;Count before;
};

LL_TYPE_INSTANCE_HOOK(FishBucketHook,ll::memory::HookPriority::Lowest,BucketItem,
    &BucketItem::$_useOn,::InteractionResult,::ItemStack& item,::Actor& target,
    ::BlockPos pos,uchar face,::HandSlot hand,::Vec3 const& click) {
    auto* ctx=context;
    if(!ctx)return origin(item,target,pos,face,hand,click);
    auto state=capture();
    bool matched=false;
    if(state && state->active.load(std::memory_order_acquire) && ctx && ctx->target==&target) {
        try {
            // Only a child of THIS Player::interact, same live target and hand.
            if(ctx->uid==target.mLegacyUniqueID->rawID && hand==HandSlot::Mainhand
                && item.mCount==1 && item.getTypeName()=="minecraft:water_bucket") {
                if(ctx->outcome.bucket_calls<2)++ctx->outcome.bucket_calls;
                matched=true;
            } else ctx->outcome.nested=true; // ambiguous branch, fail closed
        } catch (...) {ctx->outcome.nested=true;}
    }
    auto result=origin(item,target,pos,face,hand,click);
    if(matched)ctx->outcome.bucket_swing=result.mSwing;
    return result;
}

LL_TYPE_INSTANCE_HOOK(FishInteractionHook,ll::memory::HookPriority::Lowest,Player,
    &Player::interact,::InteractionResult,::Actor& target,::Vec3 const& location,::HandSlot hand) {
    if(depth){if(context)context->outcome.nested=true;CallScope suspend(nullptr);return origin(target,location,hand);}
    auto state=capture();
    if(!state || !state->active.load(std::memory_order_acquire) || hand!=HandSlot::Mainhand)return origin(target,location,hand);
    Context ctx;CallScope call(&ctx);
    return observe_fish_interaction([&]() -> std::optional<Entry> {
        std::string type=target.getTypeName();auto item=fish_bucket_item(type);
        if(item.empty() || isDead() || target.mRemoved || &getLevel()!=&target.getLevel())return {};
        auto const& uuid=getUuid();auto uid=target.mLegacyUniqueID->rawID;
        if((uuid.a==0 && uuid.b==0) || uid==0)return {};
        auto const& inventory=getInventory();auto before=count(inventory,item);if(!before)return {};
        auto name=getRealName();if(name.size()>256)name.resize(256);
        ctx.target=&target;ctx.uid=uid;
        return Entry{getEntityContext().getWeakRef(),target.getEntityContext().getWeakRef(),this,&target,
            &inventory,&getLevel(),static_cast<int>(getDimensionId()),{ActorKind::Player,"uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b),std::move(name)},std::move(type),*before};
    },[&]{return origin(target,location,hand);},[&](Entry& e,InteractionResult const& result) -> std::optional<AnalyticsEvent> {
        if(!state->active.load(std::memory_order_acquire))return {};
        auto player=e.player.tryUnwrap<Player>();if(!player || player.as_ptr()!=e.identity || player->isDead())return {};
        auto const& inventory=player->getInventory();if(&inventory!=e.inventory)return {};
        auto after=count(inventory,fish_bucket_item(e.type));if(!after)return {};
        // Re-resolve the weak target, never read the original reference after
        // origin: successful bucket capture may already have destroyed it.
        auto remaining=e.target.tryUnwrap<::Actor>();
        bool const retired=!remaining || (remaining.as_ptr()==e.target_identity && remaining->mRemoved);
        auto outcome=ctx.outcome;
        outcome.interaction_success=result.mSuccess;outcome.target_retired=retired;
        outcome.target_unavailable=!remaining; // distinguish expiry from an observed removed flag
        outcome.same_player=true;outcome.same_inventory=true;
        outcome.same_dimension=&player->getLevel()==e.level && static_cast<int>(player->getDimensionId())==e.dimension;
        outcome.slots_before=e.before.slots;outcome.slots_after=after->slots;
        outcome.before_units=e.before.units;outcome.after_units=after->units;
        auto at=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return fish_bucket_event(e.actor,e.type,outcome,at);
    },[&](AnalyticsEvent value){if(state->active.load(std::memory_order_acquire))state->sink(std::move(value));});
}
} // namespace
FishToken start_fish_capture(FishSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);if(failed || capture())return {};
        if(!installed){
            if(FishBucketHook::hook()!=0){failed=true;return {};}
            if(FishInteractionHook::hook()!=0){failed=true;return {};} // inactive hook remains harmless, no sink
            installed=true;
        }
        auto token=std::make_shared<FishCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_fish_capture(FishToken const& token) noexcept {
    if(!token)return;token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
