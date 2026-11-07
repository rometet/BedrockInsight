#include "bedrock_insight/levilamina/player_jump.hpp"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJumpEvent.h"
#include "mc/world/actor/player/Player.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {

// LK-Stats v0.13.0 counts the LeviLamina PlayerJumpEvent. The previous
// Mob::jumpFromGround/getPosDelta pre/post snapshot never produced an event in
// the verified 26.60 real-player run. It is not a reliable jump detector.
class JumpCapture final {
public:
    explicit JumpCapture(JumpSink fn) : sink(std::move(fn)) {}
    JumpSink sink;
    ll::event::ListenerPtr listener;
    std::atomic<bool> active{true};
};

namespace {
std::mutex current_mutex;
std::mutex install_mutex;
JumpToken current;

JumpToken current_capture() noexcept {
    try {
        std::lock_guard lock(current_mutex);
        return current;
    } catch (...) {
        return {};
    }
}

void on_jump(ll::event::player::PlayerJumpEvent& event,
             std::weak_ptr<JumpCapture> const& weak) noexcept {
    auto capture = weak.lock();
    if (!capture || !capture->active.load(std::memory_order_acquire)) return;
    try {
        // Same event as upstream LK-Stats. LeviLamina's 26.60 emitter
        // publishes after handling authenticated input and checking that the
        // player is on the ground with a positive vertical position delta.
        // It is an event observation, NOT independent proof of a completed
        // jump (a later event listener can cancel it).
        auto& player = event.self();
        auto const& uuid = player.getUuid();
        if (uuid.a == 0 && uuid.b == 0) return;
        AnalyticsEvent value;
        value.kind = EventKind::PlayerJumpImpulse;
        value.actor.kind = ActorKind::Player;
        value.actor.id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
        value.actor.display_name = player.getRealName();
        if (value.actor.display_name.size() > 256) value.actor.display_name.resize(256);
        value.occurred_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        value.amount = 1;
        value.primary_type = "minecraft:player";
        value.cause = "ll26.60.player_jump_event.v1";
        value.details_json =
            R"({"hook":"LeviLamina::PlayerJumpEvent","phase":"post_auth_input","grounded_and_ascending_checked_by_emitter":true,"final_world_jump_verified":false})";
        if (capture->active.load(std::memory_order_acquire)) {
            capture->sink(std::move(value));
        }
    } catch (...) {
        // An event callback must not throw into the server's event dispatcher.
    }
}
} // namespace

JumpToken start_jump_capture(JumpSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if (current_capture()) return {};
        auto state = std::make_shared<JumpCapture>(std::move(sink));
        std::weak_ptr<JumpCapture> weak{state};
        auto listener = ll::event::EventBus::getInstance()
            .emplaceListener<ll::event::player::PlayerJumpEvent>(
                [weak](ll::event::player::PlayerJumpEvent& event) {
                    on_jump(event, weak);
                });
        if (!listener) return {};
        state->listener = std::move(listener);
        {
            std::lock_guard current_lock(current_mutex);
            current = state;
        }
        return state;
    } catch (...) {
        return {};
    }
}

void stop_jump_capture(JumpToken const& token) noexcept {
    if (!token) return;
    token->active.store(false, std::memory_order_release);
    try {
        std::lock_guard lock(install_mutex);
        if (token->listener) {
            ll::event::EventBus::getInstance().removeListener(token->listener);
            token->listener.reset();
        }
        std::lock_guard current_lock(current_mutex);
        if (current == token) current.reset();
    } catch (...) {
        // The weak callback is inert once the capture is disabled.
    }
}
} // namespace bedrock_insight::levilamina
