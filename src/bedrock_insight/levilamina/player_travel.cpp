#include "bedrock_insight/levilamina/player_travel.hpp"
#include "bedrock_insight/core/travel_event.hpp"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/deps/shared_types/v1_26_20/block/MaterialType.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include <chrono>
#include <unordered_map>

namespace bedrock_insight::levilamina {
namespace {
constexpr std::size_t kCapacity = 256;

struct Entry final {
    WeakEntityRef weak;
    ::Actor const* identity = nullptr;
    Level const* level = nullptr;
    Actor actor;
    TravelWindow window;
};

void emit_entry(Entry& entry, TravelEventSink sink) {
    auto const pending = entry.window.drain();
    if (sink.emit == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < pending.size(); ++index) {
        auto const& bucket = pending[index];
        auto const cm = bucket.whole_cm();
        if (cm <= 0) continue;
        // Last observed movement, NOT an old flush time or the later shutdown.
        auto value = travel_window_event(entry.actor, static_cast<TravelMode>(index), bucket);
        if (value) try { sink.emit(sink.context, std::move(*value)); } catch (...) {}
    }
}

std::string player_id(Player const& player) {
    auto const& uuid = player.getUuid();
    if (uuid.a == 0 && uuid.b == 0) return {};
    return "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
}
} // namespace

class TravelTracker final {
public:
    std::unordered_map<std::string, Entry> entries;
};

TravelToken make_travel_tracker() { return std::make_shared<TravelTracker>(); }

void track_travel(TravelTracker& tracker, Player& player, std::int64_t utc_ms, TravelEventSink sink) {
    try {
        if (utc_ms < 0) return;
        auto id = player_id(player);
        if (id.empty()) return;
        auto it = tracker.entries.find(id);
        if (it == tracker.entries.end()) {
            if (tracker.entries.size() >= kCapacity) return;
            auto name = player.getRealName();
            if (name.size() > 256) name.resize(256);
            Entry entry{player.getEntityContext().getWeakRef(), static_cast<::Actor*>(&player),
                &player.getLevel(), {ActorKind::Player, id, name}, {}};
            tracker.entries.emplace(std::move(id), std::move(entry));
            return;
        }
        // A different entity with the same UUID must not inherit a stale delta.
        if (it->second.identity != static_cast<::Actor*>(&player)) {
            emit_entry(it->second, sink);
            it->second.identity = static_cast<::Actor*>(&player);
            it->second.weak = player.getEntityContext().getWeakRef();
            it->second.level = &player.getLevel();
            it->second.window = {};
        }
        // Re-observation never manufactures a position delta or carries an old
        // entity's baseline into a new one with the same account UUID.
        it->second.actor.display_name = player.getRealName();
        if (it->second.actor.display_name.size() > 256) it->second.actor.display_name.resize(256);
    } catch (...) {}
}

void forget_travel(TravelTracker& tracker, std::string const& id, std::int64_t utc_ms, TravelEventSink sink) {
    auto it = tracker.entries.find(id);
    if (it == tracker.entries.end()) return;
    (void)utc_ms; // attribution uses each bucket's last actual observation.
    emit_entry(it->second, sink);
    tracker.entries.erase(it);
}

void poll_travel(TravelTracker& tracker, Level& level, std::int64_t utc_ms, TravelEventSink sink) {
    if (utc_ms < 0) return;
    auto const monotonic = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    for (auto it = tracker.entries.begin(); it != tracker.entries.end();) {
        auto& entry = it->second;
        if (entry.level != &level) { ++it; continue; }
        auto actor = entry.weak.tryUnwrap<Player>();
        if (!actor || actor.as_ptr() != entry.identity) {
            // Disconnected/removed actors must not keep collected distance.
            emit_entry(entry, sink);
            it = tracker.entries.erase(it);
            continue;
        }
        try {
            auto const& pos = actor->getPosition();
            TravelFrame frame;
            frame.x = pos.x; frame.y = pos.y; frame.z = pos.z;
            frame.utc_ms = utc_ms; frame.steady_us = monotonic;
            frame.dimension = static_cast<int>(actor->getDimensionId());
            frame.alive = !actor->isDead(); // pinned LL accessor, no guessed slot.
            MovementFlags flags;
            flags.sprinting = actor->isSprinting();
            flags.sneaking = actor->isSneaking();
            flags.swimming = actor->isSwimming();
            flags.gliding = actor->isGliding();
            flags.flying = actor->isFlying();
            flags.in_water = actor->isInWater();
            flags.on_ground = actor->isOnGround();
            // Water-surface vs underwater is a distinct vanilla split. Only a
            // player actually in water contributes either key.
            flags.under_water = flags.in_water
                && actor->isUnderLiquid(::SharedTypes::v1_26_20::MaterialType::Water);
            ::Actor* vehicle = actor->getVehicle();
            if (vehicle != nullptr) {
                flags.riding = true;
                frame.mounted = true;
                frame.vehicle_id = vehicle->mLegacyUniqueID->rawID;
                auto const& name = vehicle->getTypeName();
                if (valid_vehicle_type(name)) flags.vehicle_type = name;
            }
            // Pinned26.60 Actor.h declares this const MCAPI. Use the engine's
            // own predicate, not block-name heuristics or a guessed flag bit.
            if (!flags.riding && !flags.gliding && !flags.flying && !flags.in_water && entry.window.ascending(frame.y)) {
                flags.climbing = actor->onClimbableBlock();
            }
            frame.mode = classify_travel(flags);
            if (entry.window.boundary(frame)) {
                emit_entry(entry, sink);
                entry.window.reset(frame);
            } else {
                entry.window.observe(frame);
                if (entry.window.due(monotonic)) emit_entry(entry, sink);
            }
        } catch (...) {
            emit_entry(entry, sink);
            it = tracker.entries.erase(it);
            continue;
        }
        ++it;
    }
}

void finish_travel(TravelTracker& tracker, TravelEventSink sink) {
    for (auto& [id, entry] : tracker.entries) {
        (void)id;
        emit_entry(entry, sink);
    }
    tracker.entries.clear();
}

} // namespace bedrock_insight::levilamina
