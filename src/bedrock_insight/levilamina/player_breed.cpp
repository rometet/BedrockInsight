#include "bedrock_insight/levilamina/player_breed.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/entity/components_json_legacy/BreedableComponent.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace bedrock_insight::levilamina {
class BreedCapture final {
public:
    explicit BreedCapture(BreedSink fn) : sink(std::move(fn)) {}
    BreedSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex, install_mutex;
BreedToken current;
bool installed = false, install_failed = false;
thread_local bool inside_breed = false, nested_breed = false;

BreedToken current_capture() noexcept {
    try { std::lock_guard lock(current_mutex); return current; } catch (...) { return {}; }
}
struct BreedScope final {
    BreedScope() { inside_breed = true; nested_breed = false; }
    ~BreedScope() { inside_breed = false; nested_breed = false; }
};
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// BreedableComponent::mate is MCAPI in the pinned 26.60 headers. The love cause
// (the player who fed the animal) is copied BEFORE origin because a successful
// mating may clear it. Offspring are a real success signal; a bare feeding
// attempt never calls mate or produces offspring.
LL_TYPE_INSTANCE_HOOK(BreedMateHook, ll::memory::HookPriority::Lowest, BreedableComponent,
    &BreedableComponent::mate, ::std::vector<::WeakEntityRef>, ::Actor& owner, ::Actor& partner) {
    if (inside_breed) { nested_breed = true; return origin(owner, partner); }
    auto state = current_capture();
    if (!state || !state->active.load(std::memory_order_acquire)) return origin(owner, partner);
    BreedScope scope;
    ::ActorUniqueID cause{};
    Level* level = nullptr;
    std::string owner_type;
    try {
        cause = mLoveCause;
        level = &owner.getLevel();
        owner_type = owner.getTypeName();
        if (!valid_item_type(owner_type)) owner_type.clear();
    } catch (...) {
        cause = ::ActorUniqueID{};
    }
    auto offspring = origin(owner, partner);
    if (nested_breed || cause.rawID == 0 || level == nullptr || offspring.empty()
        || !state->active.load(std::memory_order_acquire)) return offspring;
    try {
        auto* actor = level->fetchEntity(cause, false);
        if (actor == nullptr) return offspring;
        auto* player = Player::tryGetFromEntity(actor->getEntityContext(), false);
        if (player == nullptr || static_cast<::Actor*>(player) != actor) return offspring;
        auto const& uuid = player->getUuid();
        if (uuid.a == 0 && uuid.b == 0) return offspring;
        AnalyticsEvent value;
        value.kind = EventKind::PlayerInteractionResult;
        value.actor.kind = ActorKind::Player;
        value.actor.id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
        value.actor.display_name = player->getRealName();
        if (value.actor.display_name.size() > 256) value.actor.display_name.resize(256);
        value.primary_type = "minecraft:animals_bred";
        value.secondary_type = std::move(owner_type);
        value.cause = "ll26.60.breed_offspring.v1";
        value.amount = 1;
        value.occurred_at_ms = now_ms();
        value.details_json = "{\"hook\":\"BreedableComponent::mate\",\"phase\":\"post_origin_offspring\","
            "\"offspring\":" + std::to_string(offspring.size()) + ",\"love_cause_resolved\":true}";
        if (state->active.load(std::memory_order_acquire)) state->sink(std::move(value));
    } catch (...) {}
    return offspring;
}
} // namespace

BreedToken start_breed_capture(BreedSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if (install_failed || current_capture()) return {};
        if (!installed) {
            if (BreedMateHook::hook() != 0) { install_failed = true; return {}; }
            installed = true;
        }
        auto token = std::make_shared<BreedCapture>(std::move(sink));
        { std::lock_guard lock(current_mutex); current = token; }
        return token;
    } catch (...) { return {}; }
}
void stop_breed_capture(BreedToken const& token) noexcept {
    if (!token) return;
    token->active.store(false, std::memory_order_release);
    try { std::lock_guard lock(current_mutex); if (current == token) current.reset(); } catch (...) {}
}
} // namespace bedrock_insight::levilamina
