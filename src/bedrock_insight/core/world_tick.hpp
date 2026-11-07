#pragma once
#include "bedrock_insight/core/event.hpp"
#include <limits>
#include <optional>

namespace bedrock_insight {
// A copied engine tick ID, not an elapsed wall-time estimate or a sum of
// telemetry windows. No Player/Level pointer crosses the writer boundary.
inline std::optional<AnalyticsEvent> world_tick_snapshot(std::uint64_t tick,
    std::int64_t at_ms, std::string const& run_id) {
    if (tick > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
        || at_ms < 0 || run_id.empty() || run_id.size() > 128) return {};
    AnalyticsEvent value;
    value.kind = EventKind::ServerTick;
    value.actor = {ActorKind::System, run_id, ""};
    value.session_id = run_id;
    value.primary_type = "minecraft:total_world_time";
    value.occurred_at_ms = at_ms;
    value.amount = static_cast<std::int64_t>(tick);
    value.cause = "ll26.60.world_tick_snapshot.v1";
    value.details_json = "{\"adapter\":\"Level::getCurrentServerTick\",\"phase\":\"engine_tick_snapshot\","
        "\"scope\":\"world_shared\",\"aggregation\":\"latest_snapshot\",\"tick_id\":"
        + std::to_string(tick) + ",\"observed_at_ms\":" + std::to_string(at_ms)
        + ",\"sample_interval_ms\":5000,\"player_time\":false,\"restart_continuity_verified\":false}";
    return value;
}
} // namespace bedrock_insight
