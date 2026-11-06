#include "bedrock_insight/core/metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace bedrock_insight {
namespace {

std::int64_t positive_amount(AnalyticsEvent const& event) noexcept {
    return event.amount > 0 ? event.amount : 0;
}

std::tm utc_time(std::int64_t timestamp_ms) noexcept {
    auto seconds = static_cast<std::time_t>(timestamp_ms / 1000);
    auto* converted = std::gmtime(&seconds);
    if (converted == nullptr) {
        return {};
    }
    return *converted;
}

} // namespace

double ServerMetrics::average_mspt() const noexcept {
    if (tick_samples == 0) {
        return 0.0;
    }
    return static_cast<double>(total_mspt) / static_cast<double>(tick_samples);
}

double ServerMetrics::tps() const noexcept {
    auto average = average_mspt();
    if (average <= 0.0) {
        return 0.0;
    }
    return std::min(20.0, 1000.0 / average);
}

void MetricsAggregator::observe(AnalyticsEvent const& event) {
    if (event.kind == EventKind::ServerTick) {
        return;
    }
    if (!event.has_player_actor()) {
        return;
    }

    std::lock_guard lock(mutex_);
    auto& stats = players_[event.actor.id];
    if (stats.player_id.empty()) {
        stats.player_id = event.actor.id;
        stats.first_seen_ms = event.occurred_at_ms;
    }
    stats.display_name = event.actor.display_name;
    stats.last_seen_ms = std::max(stats.last_seen_ms, event.occurred_at_ms);
    add_event(stats, event);

    auto key = period_key(RankingPeriod::Day, event.occurred_at_ms);
    add_event(period_counters_[BucketKey{event.actor.id, std::move(key)}], event);
    key = period_key(RankingPeriod::Month, event.occurred_at_ms);
    add_event(period_counters_[BucketKey{event.actor.id, std::move(key)}], event);
}

void MetricsAggregator::observe_server_tick(ServerTickSample sample) {
    if (sample.mspt <= 0 || sample.online_players < 0) {
        return;
    }

    std::lock_guard lock(mutex_);
    ++server_.tick_samples;
    server_.total_mspt += sample.mspt;
    server_.max_mspt = std::max(server_.max_mspt, sample.mspt);
    server_.current_online = sample.online_players;
    server_.peak_online = std::max(server_.peak_online, sample.online_players);
    server_.last_sample_ms = sample.occurred_at_ms;
}

MetricsSnapshot MetricsAggregator::snapshot() const {
    std::lock_guard lock(mutex_);
    return {players_, server_};
}

std::vector<RankingEntry> MetricsAggregator::ranking(
    RankingMetric metric,
    RankingPeriod period,
    std::int64_t anchor_ms,
    std::size_t limit) const {
    std::vector<RankingEntry> result;
    if (limit == 0) {
        return result;
    }

    std::lock_guard lock(mutex_);
    auto key = period_key(period, anchor_ms);
    result.reserve(players_.size());
    for (auto const& [player_id, stats] : players_) {
        auto value = [&] {
            if (period == RankingPeriod::AllTime) {
                return metric_value(stats, metric);
            }
            auto bucket = period_counters_.find(BucketKey{player_id, key});
            return bucket == period_counters_.end() ? 0 : metric_value(bucket->second, metric);
        }();
        if (value > 0) {
            result.push_back({player_id, stats.display_name, value});
        }
    }

    std::sort(result.begin(), result.end(), [](RankingEntry const& left, RankingEntry const& right) {
        if (left.value != right.value) {
            return left.value > right.value;
        }
        return left.player_id < right.player_id;
    });
    if (result.size() > limit) {
        result.resize(limit);
    }
    return result;
}

std::string MetricsAggregator::period_key(RankingPeriod period, std::int64_t timestamp_ms) {
    if (period == RankingPeriod::AllTime) {
        return {};
    }
    auto value = utc_time(timestamp_ms);
    char buffer[16]{};
    if (period == RankingPeriod::Day) {
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", value.tm_year + 1900, value.tm_mon + 1, value.tm_mday);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d", value.tm_year + 1900, value.tm_mon + 1);
    }
    return buffer;
}

std::int64_t MetricsAggregator::metric_value(CounterSet const& counters, RankingMetric metric) noexcept {
    switch (metric) {
    case RankingMetric::BlocksBroken: return counters.blocks_broken;
    case RankingMetric::BlocksPlaced: return counters.blocks_placed;
    case RankingMetric::ItemsCrafted: return counters.items_crafted;
    case RankingMetric::ItemsUsed: return counters.items_used;
    case RankingMetric::Kills: return counters.kills;
    case RankingMetric::Deaths: return counters.deaths;
    case RankingMetric::DistanceM: return counters.distance_m;
    case RankingMetric::Sessions: return counters.sessions;
    }
    return 0;
}

std::int64_t MetricsAggregator::metric_value(PlayerStats const& stats, RankingMetric metric) noexcept {
    switch (metric) {
    case RankingMetric::BlocksBroken: return stats.blocks_broken;
    case RankingMetric::BlocksPlaced: return stats.blocks_placed;
    case RankingMetric::ItemsCrafted: return stats.items_crafted;
    case RankingMetric::ItemsUsed: return stats.items_used;
    case RankingMetric::Kills: return stats.kills;
    case RankingMetric::Deaths: return stats.deaths;
    case RankingMetric::DistanceM: return stats.distance_m;
    case RankingMetric::Sessions: return stats.sessions;
    }
    return 0;
}

void MetricsAggregator::add_event(CounterSet& counters, AnalyticsEvent const& event) {
    auto amount = positive_amount(event);
    switch (event.kind) {
    case EventKind::BlockBreak: counters.blocks_broken += amount; break;
    case EventKind::BlockPlace: counters.blocks_placed += amount; break;
    case EventKind::ItemCraft: counters.items_crafted += amount; break;
    case EventKind::ItemUse: counters.items_used += amount; break;
    case EventKind::EntityKill: counters.kills += amount; break;
    case EventKind::PlayerDeath: counters.deaths += amount; break;
    case EventKind::Distance: counters.distance_m += amount; break;
    case EventKind::SessionStart: ++counters.sessions; break;
    default: break;
    }
}

void MetricsAggregator::add_event(PlayerStats& stats, AnalyticsEvent const& event) {
    auto amount = positive_amount(event);
    switch (event.kind) {
    case EventKind::BlockBreak:
        stats.blocks_broken += amount;
        if (!event.primary_type.empty()) {
            stats.blocks_broken_by_type[event.primary_type] += amount;
        }
        break;
    case EventKind::BlockPlace:
        stats.blocks_placed += amount;
        if (!event.primary_type.empty()) {
            stats.blocks_placed_by_type[event.primary_type] += amount;
        }
        break;
    case EventKind::ItemCraft:
        stats.items_crafted += amount;
        if (!event.primary_type.empty()) {
            stats.items_crafted_by_type[event.primary_type] += amount;
        }
        break;
    case EventKind::ItemUse:
        stats.items_used += amount;
        if (!event.primary_type.empty()) {
            stats.items_used_by_type[event.primary_type] += amount;
        }
        break;
    case EventKind::EntityKill:
        stats.kills += amount;
        if (!event.primary_type.empty()) {
            stats.kills_by_type[event.primary_type] += amount;
        }
        break;
    case EventKind::PlayerDeath: stats.deaths += amount; break;
    case EventKind::Distance: stats.distance_m += amount; break;
    case EventKind::SessionStart:
        ++stats.sessions;
        stats.online = true;
        break;
    case EventKind::SessionEnd: stats.online = false; break;
    default: break;
    }
}

} // namespace bedrock_insight
