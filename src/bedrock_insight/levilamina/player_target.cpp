#include "bedrock_insight/levilamina/player_target.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/TargetBlock.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace bedrock_insight::levilamina {
class TargetCapture final {
public:
    explicit TargetCapture(TargetSink fn) : sink(std::move(fn)) {}
    TargetSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex, install_mutex;
TargetToken current;
bool installed = false, install_failed = false;
thread_local bool inside_target = false, nested_target = false;

TargetToken current_capture() noexcept {
    try { std::lock_guard lock(current_mutex); return current; } catch (...) { return {}; }
}
struct TargetScope final {
    TargetScope() { inside_target = true; nested_target = false; }
    ~TargetScope() { inside_target = false; nested_target = false; }
};
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// A target block only fires this when a projectile actually strikes it. The
// projectile owner is read before origin and resolved to a Player only after the
// original call. No action is counted for unowned/environment projectiles.
LL_TYPE_INSTANCE_HOOK(TargetHitHook, ll::memory::HookPriority::Lowest, TargetBlock,
    &TargetBlock::$onProjectileHit, void, ::BlockSource& region, ::BlockPos const& pos,
    ::Actor const& projectile) {
    if (inside_target) { nested_target = true; origin(region, pos, projectile); return; }
    auto state = current_capture();
    if (!state || !state->active.load(std::memory_order_acquire)) { origin(region, pos, projectile); return; }
    TargetScope scope;
    ::Actor* owner = nullptr;
    std::string projectile_type;
    try {
        owner = projectile.getOwner();
        projectile_type = projectile.getTypeName();
    } catch (...) {
        owner = nullptr;
    }
    origin(region, pos, projectile);
    if (nested_target || owner == nullptr || !state->active.load(std::memory_order_acquire)) return;
    try {
        auto* player = Player::tryGetFromEntity(owner->getEntityContext(), false);
        if (player == nullptr || static_cast<::Actor*>(player) != owner) return;
        auto const& uuid = player->getUuid();
        if (uuid.a == 0 && uuid.b == 0) return;
        AnalyticsEvent value;
        value.kind = EventKind::PlayerInteractionResult;
        value.actor.kind = ActorKind::Player;
        value.actor.id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
        value.actor.display_name = player->getRealName();
        if (value.actor.display_name.size() > 256) value.actor.display_name.resize(256);
        value.primary_type = "minecraft:target_hit";
        value.secondary_type = std::move(projectile_type);
        value.occurred_at_ms = now_ms();
        value.position = {pos.x, pos.y, pos.z,
            static_cast<std::int8_t>(static_cast<int>(player->getDimensionId()))};
        value.cause = "ll26.60.target_block_projectile_hit.v1";
        value.amount = 1;
        value.details_json = "{\"hook\":\"TargetBlock::$onProjectileHit\",\"phase\":\"post_origin_hit\","
            "\"owner_resolved_player\":true,\"projectile_type\":\"" + value.secondary_type + "\"}";
        if (state->active.load(std::memory_order_acquire)) state->sink(std::move(value));
    } catch (...) {}
}
} // namespace

TargetToken start_target_capture(TargetSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if (install_failed || current_capture()) return {};
        if (!installed) {
            if (TargetHitHook::hook() != 0) { install_failed = true; return {}; }
            installed = true;
        }
        auto token = std::make_shared<TargetCapture>(std::move(sink));
        { std::lock_guard lock(current_mutex); current = token; }
        return token;
    } catch (...) { return {}; }
}
void stop_target_capture(TargetToken const& token) noexcept {
    if (!token) return;
    token->active.store(false, std::memory_order_release);
    try { std::lock_guard lock(current_mutex); if (current == token) current.reset(); } catch (...) {}
}
} // namespace bedrock_insight::levilamina
