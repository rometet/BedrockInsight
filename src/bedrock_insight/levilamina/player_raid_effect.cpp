#include "bedrock_insight/levilamina/player_raid_effect.hpp"
#include "bedrock_insight/core/raid_effect.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/effect/MobEffectIds.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class RaidEffectCapture {
public:
    explicit RaidEffectCapture(RaidEffectSink fn):sink(std::move(fn)){}
    RaidEffectSink sink;std::atomic<bool> active{true};
};
namespace {
static_assert(static_cast<int>(MobEffectIds::VillageHero)==29 && static_cast<int>(MobEffectIds::RaidOmen)==36);
std::mutex current_mutex,install_mutex;
RaidEffectToken current;bool installed=false,failed=false;
RaidEffectToken capture() noexcept {try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}}
thread_local bool* outer_nested=nullptr;
struct CallScope {
    bool* saved;
    explicit CallScope(bool* current):saved(outer_nested){outer_nested=current;}
    ~CallScope(){outer_nested=saved;}
};
RaidEffectState state_of(MobEffectInstance const& e){return {static_cast<int>(e.mId),e.mDuration->mValue,e.mAmplifier};}
struct Entry {
    WeakEntityRef weak;Player const* identity;Level const* level;int dimension;
    Actor actor;RaidEffectOutcome outcome;
};
LL_TYPE_INSTANCE_HOOK(RaidEffectHook,ll::memory::HookPriority::Lowest,::Actor,
    &::Actor::addEffect,void,::MobEffectInstance const& effect) {
    if(outer_nested){*outer_nested=true;return origin(effect);}
    auto state=capture();if(!state || !state->active.load(std::memory_order_acquire))return origin(effect);
    bool nested=false;CallScope scope(&nested);
    observe_raid_effect([&]() -> std::optional<Entry> {
        auto requested=state_of(effect);if(raid_effect_key(requested).empty())return {};
        auto weak=getEntityContext().getWeakRef();auto player=weak.tryUnwrap<Player>();
        if(!player || player->isDead() || player->mRemoved)return {};
        auto const& uuid=player->getUuid();if(uuid.a==0 && uuid.b==0)return {};
        RaidEffectOutcome outcome;outcome.requested=requested;
        if(auto before=player->getEffect(static_cast<uint>(requested.id)))outcome.before=state_of(*before);
        auto name=player->getRealName();if(name.size()>256)name.resize(256);
        return Entry{std::move(weak),player.as_ptr(),&player->getLevel(),static_cast<int>(player->getDimensionId()),
            {ActorKind::Player,"uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b),std::move(name)},outcome};
    },[&]{origin(effect);},[&](Entry& e) -> std::optional<AnalyticsEvent> {
        if(!state->active.load(std::memory_order_acquire) || nested)return {};
        auto player=e.weak.tryUnwrap<Player>();
        if(!player || player.as_ptr()!=e.identity || player->isDead() || player->mRemoved)return {};
        auto after=player->getEffect(static_cast<uint>(e.outcome.requested.id));if(!after)return {};
        auto o=e.outcome;o.after=state_of(*after);o.same_player=true;o.nested=nested;
        o.same_level=&player->getLevel()==e.level && static_cast<int>(player->getDimensionId())==e.dimension;
        auto at=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return raid_effect_event(e.actor,o,at);
    },[&](AnalyticsEvent v){if(state->active.load(std::memory_order_acquire))state->sink(std::move(v));});
}
}
RaidEffectToken start_raid_effect_capture(RaidEffectSink sink) noexcept {
    try{std::lock_guard lock(install_mutex);if(failed || capture())return {};
        if(!installed){if(RaidEffectHook::hook()!=0){failed=true;return {};}installed=true;}
        auto token=std::make_shared<RaidEffectCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_raid_effect_capture(RaidEffectToken const& token) noexcept {
    if(!token)return;token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
