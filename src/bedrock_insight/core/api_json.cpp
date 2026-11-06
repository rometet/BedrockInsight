#include "bedrock_insight/core/api_json.hpp"

#include <array>
#include <iomanip>
#include <locale>
#include <sstream>

namespace bedrock_insight {
namespace {

void append_key(std::string& output, std::string_view key) {
    if (output.back() != '{' && output.back() != '[') {
        output.push_back(',');
    }
    output += json_quote(key);
    output.push_back(':');
}

void append_string(std::string& output, std::string_view key, std::string_view value) {
    append_key(output, key);
    output += json_quote(value);
}

void append_integer(std::string& output, std::string_view key, std::int64_t value) {
    append_key(output, key);
    output += std::to_string(value);
}

void append_unsigned(std::string& output, std::string_view key, std::uint64_t value) {
    append_key(output, key);
    output += std::to_string(value);
}

void append_boolean(std::string& output, std::string_view key, bool value) {
    append_key(output, key);
    output += value ? "true" : "false";
}

void append_double(std::string& output, std::string_view key, double value) {
    append_key(output, key);
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(10) << value;
    output += stream.str();
}

template <typename Map>
void append_counter_map(std::string& output, std::string_view key, Map const& counters) {
    append_key(output, key);
    output.push_back('{');
    for (auto const& [type, value] : counters) {
        append_key(output, type);
        output += std::to_string(value);
    }
    output.push_back('}');
}

void append_position(std::string& output, Position const& position) {
    output += "{\"x\":";
    output += std::to_string(position.x);
    output += ",\"y\":";
    output += std::to_string(position.y);
    output += ",\"z\":";
    output += std::to_string(position.z);
    output += ",\"dimension\":";
    output += std::to_string(position.dimension);
    output.push_back('}');
}

void append_audit_record(std::string& output, AuditRecord const& record) {
    output.push_back('{');
    append_unsigned(output, "event_id", record.event_id);
    append_integer(output, "occurred_at_ms", record.occurred_at_ms);
    append_string(output, "event_kind", record.event_kind);
    append_string(output, "actor_kind", record.actor_kind);
    append_string(output, "actor_id", record.actor_id);
    append_string(output, "actor_name", record.actor_name);
    append_string(output, "world_id", record.world_id);
    append_key(output, "position");
    append_position(output, record.position);
    append_string(output, "primary_type", record.primary_type);
    append_string(output, "secondary_type", record.secondary_type);
    append_string(output, "session_id", record.session_id);
    append_string(output, "cause", record.cause);
    append_integer(output, "amount", record.amount);
    append_string(output, "details_json", record.details_json);
    append_string(output, "before_type", record.before_type);
    append_string(output, "after_type", record.after_type);
    output.push_back('}');
}

} // namespace

std::string json_quote(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char character : value) {
        switch (character) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20) {
                result += "\\u00";
                result.push_back(hex[(character >> 4) & 0x0f]);
                result.push_back(hex[character & 0x0f]);
            } else {
                result.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

std::string_view ranking_metric_name(RankingMetric metric) noexcept {
    switch (metric) {
    case RankingMetric::BlocksBroken: return "blocks_broken";
    case RankingMetric::BlocksPlaced: return "blocks_placed";
    case RankingMetric::ItemsCrafted: return "items_crafted";
    case RankingMetric::ItemsUsed: return "items_used";
    case RankingMetric::Kills: return "kills";
    case RankingMetric::Deaths: return "deaths";
    case RankingMetric::DistanceM: return "distance_m";
    case RankingMetric::Sessions: return "sessions";
    }
    return "unknown";
}

std::string_view ranking_period_name(RankingPeriod period) noexcept {
    switch (period) {
    case RankingPeriod::Day: return "day";
    case RankingPeriod::Month: return "month";
    case RankingPeriod::AllTime: return "all_time";
    }
    return "unknown";
}

std::string serialize_server_metrics_json(ServerMetrics const& metrics) {
    std::string output = "{";
    append_unsigned(output, "tick_samples", metrics.tick_samples);
    append_integer(output, "total_mspt", metrics.total_mspt);
    append_integer(output, "max_mspt", metrics.max_mspt);
    append_double(output, "average_mspt", metrics.average_mspt());
    append_double(output, "tps", metrics.tps());
    append_integer(output, "current_online", metrics.current_online);
    append_integer(output, "peak_online", metrics.peak_online);
    append_integer(output, "last_sample_ms", metrics.last_sample_ms);
    output.push_back('}');
    return output;
}

std::string serialize_rankings_json(RankingMetric metric, RankingPeriod period, std::string_view period_key,
    std::vector<RankingEntry> const& entries) {
    std::string output = "{";
    append_string(output, "metric", ranking_metric_name(metric));
    append_string(output, "period", ranking_period_name(period));
    append_string(output, "period_key", period_key);
    append_key(output, "entries");
    output.push_back('[');
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index != 0) output.push_back(',');
        output.push_back('{');
        append_unsigned(output, "rank", index + 1);
        append_string(output, "player_id", entries[index].player_id);
        append_string(output, "display_name", entries[index].display_name);
        append_integer(output, "value", entries[index].value);
        output.push_back('}');
    }
    output += "]}";
    return output;
}

std::string serialize_player_stats_json(PlayerStats const& stats) {
    std::string output = "{";
    append_string(output, "player_id", stats.player_id);
    append_string(output, "display_name", stats.display_name);
    append_integer(output, "first_seen_ms", stats.first_seen_ms);
    append_integer(output, "last_seen_ms", stats.last_seen_ms);
    append_boolean(output, "online", stats.online);
    append_integer(output, "blocks_broken", stats.blocks_broken);
    append_integer(output, "blocks_placed", stats.blocks_placed);
    append_integer(output, "items_crafted", stats.items_crafted);
    append_integer(output, "items_used", stats.items_used);
    append_integer(output, "kills", stats.kills);
    append_integer(output, "deaths", stats.deaths);
    append_integer(output, "distance_m", stats.distance_m);
    append_integer(output, "sessions", stats.sessions);
    append_counter_map(output, "blocks_broken_by_type", stats.blocks_broken_by_type);
    append_counter_map(output, "blocks_placed_by_type", stats.blocks_placed_by_type);
    append_counter_map(output, "items_crafted_by_type", stats.items_crafted_by_type);
    append_counter_map(output, "items_used_by_type", stats.items_used_by_type);
    append_counter_map(output, "kills_by_type", stats.kills_by_type);
    output.push_back('}');
    return output;
}

std::string serialize_audit_records_json(std::vector<AuditRecord> const& records) {
    std::string output = "{\"records\":[";
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (index != 0) output.push_back(',');
        append_audit_record(output, records[index]);
    }
    output += "]}";
    return output;
}

std::string serialize_rollback_summary_json(RollbackSummary const& summary) {
    std::string output = "{";
    append_string(output, "transaction_id", summary.transaction_id);
    append_string(output, "operation", summary.operation);
    append_string(output, "actor_id", summary.actor_id);
    append_string(output, "reason", summary.reason);
    append_integer(output, "created_at_ms", summary.created_at_ms);
    append_string(output, "status", summary.status);
    append_integer(output, "entry_count", summary.entry_count);
    append_integer(output, "applied_count", summary.applied_count);
    append_string(output, "error", summary.error);
    output.push_back('}');
    return output;
}

std::string serialize_admin_action_log_json(std::vector<AdminActionLogEntry> const& entries) {
    std::string output = "{\"entries\":[";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index != 0) output.push_back(',');
        auto const& entry = entries[index];
        output.push_back('{');
        append_unsigned(output, "id", entry.id);
        append_integer(output, "occurred_at_ms", entry.occurred_at_ms);
        append_string(output, "actor_id", entry.actor_id);
        append_string(output, "role", to_string(entry.role));
        append_string(output, "action", entry.action);
        append_string(output, "method", entry.method);
        append_string(output, "path", entry.path);
        append_string(output, "remote_address", entry.remote_address);
        append_boolean(output, "allowed", entry.allowed);
        append_integer(output, "status", entry.status);
        append_string(output, "detail", entry.detail);
        output.push_back('}');
    }
    output += "]}";
    return output;
}

} // namespace bedrock_insight
