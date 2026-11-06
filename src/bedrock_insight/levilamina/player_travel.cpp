#include "bedrock_insight/levilamina/player_travel.hpp"
#include "bedrock_insight/core/travel_observation.hpp"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/deps/shared_types/v1_26_20/block/MaterialType.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include <map>
#include <unordered_map>

namespace bedrock_insight::levilamina {
namespace {
constexpr std::size_t kCapacity = 256;
constexpr std::int64_t kFlushMs = 5000;

struct Entry final {
    WeakEntityRef weak;
    ::Actor const* identity = nullptr;
    Level const* level = nullptr;
    Actor actor;
    Vec3 last;
    int dimension = 0;
    std::map<std::string, std::int64_t, std::less<>> pending;
    std::int64_t last_flush = 0;
};

void emit_entry(Entry& entry, std::int64_t utc_ms, TravelEventSink sink) {
    if (sink.emit == nullptr) {
        entry.pending.clear();
        return;
    }
    for (auto const& [mode, cm] : entry.pending) {
        if (cm <= 0) continue;
        AnalyticsEvent value;
        value.kind = EventKind::PlayerTravelDistance;
        value.actor = entry.actor;
        value.primary_type = mode;
        value.amount = cm;
        value.occurred_at_ms = utc_ms;
        value.cause = "ll26.60.poll_position_delta.v1";
        value.details_json = "{\"adapter\":\"ServerLevelTickEvent\",\"phase\":\"polled_position_delta\","
            "\"mode\":\"" + mode + "\",\"distance_cm\":" + std::to_string(cm)
            + ",\"flush_interval_ms\":" + std::to_string(kFlushMs) + "}";
        try { sink.emit(sink.context, std::move(value)); } catch (...) {}
    }
    entry.pending.clear();
}

void flush(Entry& entry, std::int64_t utc_ms, TravelEventSink sink) {
    emit_entry(entry, utc_ms, sink);
    entry.last_flush = utc_ms;
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

void track_travel(TravelTracker& tracker, Player& player, std::int64_t utc_ms, TravelEventSink) {
    try {
        auto id = player_id(player);
        if (id.empty()) return;
        auto it = tracker.entries.find(id);
        if (it == tracker.entries.end()) {
            if (tracker.entries.size() >= kCapacity) return;
            auto name = player.getRealName();
            if (name.size() > 256) name.resize(256);
            Entry entry{player.getEntityContext().getWeakRef(), static_cast<::Actor*>(&player),
                &player.getLevel(), {ActorKind::Player, id, name}, player.getPosition(),
                static_cast<int>(player.getDimensionId()), {}, utc_ms};
            tracker.entries.emplace(std::move(id), std::move(entry));
            return;
        }
        // A different entity with the same UUID must not inherit a stale delta.
        if (it->second.identity != static_cast<::Actor*>(&player)) {
            it->second.identity = static_cast<::Actor*>(&player);
            it->second.weak = player.getEntityContext().getWeakRef();
            it->second.level = &player.getLevel();
            it->second.dimension = static_cast<int>(player.getDimensionId());
            it->second.pending.clear();
        }
        it->second.last = player.getPosition();
        it->second.dimension = static_cast<int>(player.getDimensionId());
        it->second.actor.display_name = player.getRealName();
        if (it->second.actor.display_name.size() > 256) it->second.actor.display_name.resize(256);
    } catch (...) {}
}

void forget_travel(TravelTracker& tracker, std::string const& id, std::int64_t utc_ms, TravelEventSink sink) {
    auto it = tracker.entries.find(id);
    if (it == tracker.entries.end()) return;
    // Use the disconnect time, not a possibly 5s-stale last_flush timestamp, so
    // the final coalesced distance lands in the correct daily/period bucket.
    flush(it->second, utc_ms >= 0 ? utc_ms : it->second.last_flush, sink);
    tracker.entries.erase(it);
}

void poll_travel(TravelTracker& tracker, Level& level, std::int64_t utc_ms, TravelEventSink sink) {
    if (utc_ms < 0) return;
    for (auto it = tracker.entries.begin(); it != tracker.entries.end();) {
        auto& entry = it->second;
        if (entry.level != &level) { ++it; continue; }
        auto actor = entry.weak.tryUnwrap<Player>();
        if (!actor || actor.as_ptr() != entry.identity) {
            // Disconnected/removed actors must not keep collected distance.
            flush(entry, utc_ms, sink);
            it = tracker.entries.erase(it);
            continue;
        }
        try {
            auto const& pos = actor->getPosition();
            int const dimension = static_cast<int>(actor->getDimensionId());
            if (dimension != entry.dimension) {
                // A portal/respawn baseline is not travel distance.
                entry.dimension = dimension;
                entry.last = pos;
                ++it;
                continue;
            }
            auto const dx = pos.x - entry.last.x;
            auto const dy = pos.y - entry.last.y;
            auto const dz = pos.z - entry.last.z;
            if (!travel_step_is_continuous(dx, dy, dz)) {
                // Teleport/pearl-scale discontinuity: reset the baseline and do
                // not fabricate the jump as walked distance.
                entry.last = pos;
                ++it;
                continue;
            }
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
            bool const under_water = flags.in_water
                && actor->isUnderLiquid(::SharedTypes::v1_26_20::MaterialType::Water);
            ::Actor* vehicle = actor->getVehicle();
            if (vehicle != nullptr) {
                flags.riding = true;
                auto const& name = vehicle->getTypeName();
                if (valid_vehicle_type(name)) flags.vehicle_type = name;
            }
            // Climbing requires an upward step out of a vanilla climbable block.
            // A block query is used instead of an unexported movement flag; it
            // only runs for ascending, non-flying, non-riding players.
            if (!flags.riding && !flags.gliding && !flags.flying && dy > 0.0) {
                try {
                    auto const feet = actor->getFeetBlockPos();
                    auto const& block = actor->getDimensionBlockSource().getBlock(feet);
                    flags.climbing = is_climbable_block_name(block.getTypeName());
                } catch (...) {}
            }
            auto const mode = classify_travel(flags);
            auto const cm = travel_distance_cm(mode, dx, dy, dz);
            entry.last = pos;
            if (cm > 0) {
                auto& total = entry.pending[std::string(travel_mode_name(mode))];
                if (total <= 100000000) total += cm;
            }
            // Additive water split: a swimming/underwater player also accrues
            // this distance under the correct surface/underwater key.
            if (flags.in_water) {
                auto const water_mode = under_water ? TravelMode::WalkUnderWater : TravelMode::WalkOnWater;
                auto const water_cm = travel_distance_cm(water_mode, dx, dy, dz);
                if (water_cm > 0) {
                    auto& total = entry.pending[std::string(travel_mode_name(water_mode))];
                    if (total <= 100000000) total += water_cm;
                }
            }
            if (utc_ms - entry.last_flush >= kFlushMs && !entry.pending.empty()) {
                flush(entry, utc_ms, sink);
            }
        } catch (...) {
            flush(entry, utc_ms, sink);
            it = tracker.entries.erase(it);
            continue;
        }
        ++it;
    }
}

void finish_travel(TravelTracker& tracker, TravelEventSink sink) {
    for (auto& [id, entry] : tracker.entries) {
        (void)id;
        flush(entry, entry.last_flush, sink);
    }
    tracker.entries.clear();
}

} // namespace bedrock_insight::levilamina
