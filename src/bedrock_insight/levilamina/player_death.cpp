#include "bedrock_insight/levilamina/player_death.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/state_transition.hpp"
#include "bedrock_insight/core/damage_attribution.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

namespace bedrock_insight::levilamina {
class DeathCapture final {
public:
    explicit DeathCapture(DeathSink fn):sink(std::move(fn)) {}
    DeathSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex, install_mutex;
DeathToken current;
bool installed=false, install_failed=false;
thread_local bool inside_death=false,nested_death=false;
DeathToken current_capture() noexcept {
    try {std::lock_guard lock(current_mutex);return current;} catch (...) {return {};}
}
struct DeathScope final {
    DeathScope() {inside_death=true;nested_death=false;}
    ~DeathScope() {inside_death=false;nested_death=false;}
};
struct DeathEntry final {
    WeakEntityRef weak;
    ::Actor const* identity;
    AnalyticsEvent event;
    std::optional<AnalyticsEvent> attributed;
};

LL_TYPE_INSTANCE_HOOK(DeathTransitionHook, ll::memory::HookPriority::Lowest, Player,
    &Player::$die, void, ActorDamageSource const& source) {
    if(inside_death){nested_death=true;origin(source);return;}
    auto state=current_capture();
    if (!state || !state->active.load(std::memory_order_acquire)) {
        origin(source);return;
    }
    DeathScope scope;
    observe_state_transition([&]() -> std::optional<DeathEntry> {
        // isDead is the pinned LL accessor for IsDeadFlagComponent, NOT an
        // unchecked isAlive MCNAPI thunk or a guessed health/offset test.
        if (!state->active.load(std::memory_order_acquire) || isDead()) return {};
        auto const& uuid=getUuid();
        if (uuid.a==0 && uuid.b==0) return {};
        DeathEntry entry{getEntityContext().getWeakRef(),static_cast<::Actor*>(this),{},std::nullopt};
        entry.event.kind=EventKind::PlayerDeath;
        entry.event.actor.kind=ActorKind::Player;
        entry.event.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        entry.event.actor.display_name=getRealName();
        if (entry.event.actor.display_name.size()>256) entry.event.actor.display_name.resize(256);
        entry.event.cause="ll26.60.player_dead_transition.v1";
        entry.event.primary_type="minecraft:player";
        int code=static_cast<int>(source.mCause);
        if (code < -1 || code > 34) code=-1; // exclude enum All/unknown values
        // Preserve the original death-cause record/reader contract. A separate
        // versioned event carries source-actor attribution, never a cause code.
        entry.event.secondary_type="cause:"+std::to_string(code);
        entry.event.details_json="{\"hook\":\"Player::$die\",\"phase\":\"post_dead_transition\",\"before_dead\":false,"
            "\"after_dead\":true,\"cause_code\":"+std::to_string(code)
            +",\"killer_attribution\":\"unmeasured\"}";
        try {
            DamageAttributionFacts f;
            f.cause=code;f.entity=source.isEntitySource();f.child=source.isChildEntitySource();
            f.block=source.isBlockSource();
            if(f.entity && !f.block && (code==2 || code==3)) {
                auto const owner=source.getEntityUniqueID();
                f.source_id=owner.rawID;f.damaging_id=source.getDamagingEntityUniqueID().rawID;
                auto& level=getLevel();
                auto* actor=f.source_id==0?nullptr:level.fetchEntity(owner,false);
                f.resolved=actor!=nullptr;f.self=actor==static_cast<::Actor*>(this);
                f.same_level=actor && &actor->getLevel()==&level;
                auto const path=resolved_damage_path(f);
                if(!path.empty()) {
                    auto type=actor->getTypeName();
                    if(valid_item_type(type)) {
                        AnalyticsEvent a=entry.event;
                        a.kind=EventKind::PlayerKilledByEntity;
                        a.cause="ll26.60.resolved_killer_dead_transition.v1";
                        a.primary_type=std::move(type);a.secondary_type=path;
                        a.details_json="{\"hook\":\"Player::$die\",\"phase\":\"post_dead_transition\","
                            "\"before_dead\":false,\"after_dead\":true,\"nested\":false,"
                            "\"source_entity\":true,\"source_block\":false,\"resolved_actor\":true,"
                            "\"same_level\":true,\"self\":false,\"world_death_verified\":false,\"source_child\":"
                            +std::string(f.child?"true":"false")+",\"damage_cause\":"+std::to_string(code)
                            +",\"source_id\":"+std::to_string(f.source_id)+",\"damaging_id\":"
                            +std::to_string(f.damaging_id)+"}";
                        entry.attributed=std::move(a);
                    }
                }
            }
        }catch(...){entry.attributed.reset();} // Attribution failure must not lose the base death.
        return entry;
    },[&] {origin(source);},[&](DeathEntry& entry) {
        // Never dereference the old Player after origin. Resolution checks the
        // registry/entity generation and excludes removed entities. Compare a
        // pointer only; do not turn a replacement actor into this player's death.
        if (nested_death || !state->active.load(std::memory_order_acquire)) return false;
        auto actor=entry.weak.tryUnwrap<::Actor>();
        return actor && actor.as_ptr()==entry.identity && actor->isDead();
    },[&](DeathEntry entry) {
        if (!state->active.load(std::memory_order_acquire)) return;
        entry.event.occurred_at_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto const at=entry.event.occurred_at_ms;
        state->sink(std::move(entry.event));
        if(entry.attributed && state->active.load(std::memory_order_acquire)) {
            entry.attributed->occurred_at_ms=at;
            state->sink(std::move(*entry.attributed));
        }
    });
}
}
DeathToken start_death_capture(DeathSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if (install_failed || current_capture()) return {};
        if (!installed) {
            if (DeathTransitionHook::hook()!=0) {install_failed=true;return {};}
            installed=true;
        }
        auto state=std::make_shared<DeathCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=state;}
        return state;
    } catch (...) {return {};}
}
void stop_death_capture(DeathToken const& token) noexcept {
    if (!token) return;
    token->active.store(false,std::memory_order_release);
    try {std::lock_guard lock(current_mutex);if(current==token)current.reset();} catch (...) {}
}
} // namespace bedrock_insight::levilamina
