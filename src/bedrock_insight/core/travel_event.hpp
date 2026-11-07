#pragma once
#include "bedrock_insight/core/event.hpp"
#include "bedrock_insight/core/travel_window.hpp"
#include <string>

namespace bedrock_insight {
// Same builder is exercised by the StorageWriter/StatsReader fixture. No engine
// object or executable upstream code is involved in this serialized contract.
inline std::optional<AnalyticsEvent> travel_window_event(Actor const& actor, TravelMode mode, TravelBucket const& b) {
    if (actor.kind != ActorKind::Player || actor.id.empty() || mode == TravelMode::RideOther
        || static_cast<std::size_t>(mode) >= kTravelModeCount || !std::isfinite(b.centimeters)
        || b.centimeters < 1 || b.centimeters > 100000000 || b.samples == 0 || b.samples > 1000000
        || b.first_ms < 0 || b.last_ms < b.first_ms || TravelWindow::jst_day(b.first_ms) != TravelWindow::jst_day(b.last_ms)) return {};
    AnalyticsEvent value;
    value.kind = EventKind::PlayerTravelDistance;
    value.actor = actor;
    value.primary_type = travel_mode_name(mode);
    value.amount = b.whole_cm();
    value.occurred_at_ms = b.last_ms;
    value.cause = "ll26.60.poll_position_delta.v2";
    value.details_json = "{\"adapter\":\"ServerLevelTickEvent\",\"phase\":\"coherent_position_window\","
        "\"clock\":\"steady\",\"mode\":\"" + value.primary_type + "\",\"distance_cm\":" + std::to_string(value.amount)
        + ",\"flush_interval_ms\":5000,\"first_ms\":" + std::to_string(b.first_ms)
        + ",\"last_ms\":" + std::to_string(b.last_ms)
        + ",\"dimension\":" + std::to_string(b.dimension)
        + ",\"sample_count\":" + std::to_string(b.samples)
        + ",\"max_gap_us\":1000000,\"max_step_cm\":1000,\"precision\":\"floor_window_cm\","
          "\"period_assignment\":\"last_observation\",\"world_movement_verified\":false}";
    return value;
}
} // namespace bedrock_insight
