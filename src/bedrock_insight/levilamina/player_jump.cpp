#include "bedrock_insight/levilamina/player_jump.hpp"
#include "bedrock_insight/core/jump_observation.hpp"
#include "bedrock_insight/core/state_transition.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class JumpCapture final {
public:
    explicit JumpCapture(JumpSink fn):sink(std::move(fn)){}
    JumpSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
JumpToken current;
bool installed=false,install_failed=false;
thread_local bool inside_jump=false,nested_jump=false;
JumpToken current_capture() noexcept {
    try {std::lock_guard lock(current_mutex);return current;}catch(...){return {};}
}
struct JumpScope final {
    JumpScope(){inside_jump=true;nested_jump=false;}
    ~JumpScope(){inside_jump=false;nested_jump=false;}
};
std::optional<JumpState> read_state(Player const& player) {
    // getVelocity is NOT used: its LL implementation reads post-tick movement
    // (or a vehicle). This is the pinned immediate state-vector accessor.
    auto y=jump_y_micro(player.getPosDelta().y);if(!y)return {};
    return JumpState{*y,static_cast<std::int32_t>(player.getDimensionId()),
        player.isOnGround(),player.getVehicle()!=nullptr,player.isDead()};
}
struct JumpEntry {
    WeakEntityRef weak;
    Player const* identity;
    JumpState before;
    AnalyticsEvent value;
};
LL_TYPE_INSTANCE_HOOK(JumpImpulseHook,ll::memory::HookPriority::Lowest,Mob,
    &Mob::jumpFromGround,void) {
    if(inside_jump){nested_jump=true;origin();return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){origin();return;}
    JumpScope scope;
    observe_state_transition([&]() -> std::optional<JumpEntry> {
        if(!state->active.load(std::memory_order_acquire))return {};
        auto* player=Player::tryGetFromEntity(getEntityContext(),false);
        if(!player || static_cast<Mob*>(player)!=this)return {};
        auto before=read_state(*player);
        if(!before || !before->grounded || before->vehicle || before->dead)return {};
        auto const& uuid=player->getUuid();if(uuid.a==0 && uuid.b==0)return {};
        JumpEntry entry{player->getEntityContext().getWeakRef(),player,*before,{}};
        entry.value.kind=EventKind::PlayerJumpImpulse;
        entry.value.actor.kind=ActorKind::Player;
        entry.value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        entry.value.actor.display_name=player->getRealName();
        if(entry.value.actor.display_name.size()>256)entry.value.actor.display_name.resize(256);
        entry.value.cause="ll26.60.jump_pos_delta.v1";
        entry.value.primary_type="minecraft:player";
        return entry;
    },[&]{origin();},[&](JumpEntry& entry) {
        if(nested_jump || !state->active.load(std::memory_order_acquire))return false;
        // Old Mob/Player is never dereferenced after origin. Reacquire the same
        // nonremoved player and reject death/mount/dimension changes.
        auto player=entry.weak.tryUnwrap<Player>();
        if(!player || player.as_ptr()!=entry.identity)return false;
        auto after=read_state(*player);
        if(!after || !observed_upward_impulse(entry.before,*after,true,nested_jump))return false;
        entry.value.details_json="{\"hook\":\"Mob::jumpFromGround\",\"phase\":\"post_upward_impulse\",\"measurement\":\"pos_delta_y_x1e6\",\"grounded_before\":true,\"vehicle_before\":false,\"vehicle_after\":false,\"before_y_micro\":"
            +std::to_string(entry.before.y_micro)+",\"after_y_micro\":"+std::to_string(after->y_micro)
            +",\"dimension\":"+std::to_string(after->dimension)+",\"world_jump_verified\":false}";
        return true;
    },[&](JumpEntry entry) {
        if(!state->active.load(std::memory_order_acquire))return;
        entry.value.occurred_at_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state->sink(std::move(entry.value));
    });
}
}
JumpToken start_jump_capture(JumpSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if(install_failed || current_capture())return {};
        if(!installed){if(JumpImpulseHook::hook()!=0){install_failed=true;return {};}installed=true;}
        auto token=std::make_shared<JumpCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}
        return token;
    }catch(...){return {};}
}
void stop_jump_capture(JumpToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try {std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
