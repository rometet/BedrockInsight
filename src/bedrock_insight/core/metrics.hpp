#pragma once

#include "bedrock_insight/core/event.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace bedrock_insight {

enum class RankingMetric : std::uint8_t {
    BlocksBroken,
    BlocksPlaced,
    ItemsCrafted,
    ItemsUsed,
    Kills,
    Deaths,
    DistanceM,
    Sessions,
};

enum class RankingPeriod : std::uint8_t {
    Day,
    Month,
    AllTime,
};

struct PlayerStats final {
    std::string player_id;
    std::string display_name;
    std::int64_t first_seen_ms = 0;
    std::int64_t last_seen_ms = 0;
    bool online = false;

    std::int64_t blocks_broken = 0;
    std::int64_t blocks_placed = 0;
    std::int64_t items_crafted = 0;
    std::int64_t items_used = 0;
    std::int64_t kills = 0;
    std::int64_t deaths = 0;
    std::int64_t distance_m = 0;
    std::int64_t sessions = 0;

    std::map<std::string, std::int64_t> blocks_broken_by_type;
    std::map<std::string, std::int64_t> blocks_placed_by_type;
    std::map<std::string, std::int64_t> items_crafted_by_type;
    std::map<std::string, std::int64_t> items_used_by_type;
    std::map<std::string, std::int64_t> kills_by_type;
};

struct ServerTickSample final {
    std::int64_t occurred_at_ms = 0;
    std::int64_t mspt = 0;
    std::int32_t online_players = 0;
};

struct ServerMetrics final {
    std::uint64_t tick_samples = 0;
    std::int64_t total_mspt = 0;
    std::int64_t max_mspt = 0;
    std::int32_t current_online = 0;
    std::int32_t peak_online = 0;
    std::int64_t last_sample_ms = 0;

    [[nodiscard]] double average_mspt() const noexcept;
    [[nodiscard]] double tps() const noexcept;
};

struct RankingEntry final {
    std::string player_id;
    std::string display_name;
    std::int64_t value = 0;
};

struct MetricsSnapshot final {
    std::map<std::string, PlayerStats> players;
    ServerMetrics server;
};

// Adapter-neutral M2 aggregation. It owns no game objects and performs no
// database I/O, so an adapter can update it from an event dispatch path and
// submit the same immutable event to StorageWriter independently.
class MetricsAggregator final {
public:
    void observe(AnalyticsEvent const& event);
    void observe_server_tick(ServerTickSample sample);

    [[nodiscard]] MetricsSnapshot snapshot() const;
    [[nodiscard]] std::vector<RankingEntry> ranking(
        RankingMetric metric,
        RankingPeriod period,
        std::int64_t anchor_ms,
        std::size_t limit = 10) const;

private:
    struct CounterSet final {
        std::int64_t blocks_broken = 0;
        std::int64_t blocks_placed = 0;
        std::int64_t items_crafted = 0;
        std::int64_t items_used = 0;
        std::int64_t kills = 0;
        std::int64_t deaths = 0;
        std::int64_t distance_m = 0;
        std::int64_t sessions = 0;
    };

    struct BucketKey final {
        std::string player_id;
        std::string period;

        friend bool operator<(BucketKey const& left, BucketKey const& right) noexcept {
            return left.player_id < right.player_id
                || (left.player_id == right.player_id && left.period < right.period);
        }
    };

    static std::string period_key(RankingPeriod period, std::int64_t timestamp_ms);
    static std::int64_t metric_value(CounterSet const& counters, RankingMetric metric) noexcept;
    static std::int64_t metric_value(PlayerStats const& stats, RankingMetric metric) noexcept;
    static void add_event(CounterSet& counters, AnalyticsEvent const& event);
    static void add_event(PlayerStats& stats, AnalyticsEvent const& event);

    mutable std::mutex mutex_;
    std::map<std::string, PlayerStats> players_;
    std::map<BucketKey, CounterSet> period_counters_;
    ServerMetrics server_;
};

} // namespace bedrock_insight
