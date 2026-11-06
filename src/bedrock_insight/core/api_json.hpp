#pragma once

#include "bedrock_insight/core/admin_api.hpp"
#include "bedrock_insight/core/analytics_query.hpp"
#include "bedrock_insight/core/metrics.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace bedrock_insight {

std::string json_quote(std::string_view value);
std::string_view ranking_metric_name(RankingMetric metric) noexcept;
std::string_view ranking_period_name(RankingPeriod period) noexcept;

std::string serialize_server_metrics_json(ServerMetrics const& metrics);
std::string serialize_rankings_json(RankingMetric metric, RankingPeriod period, std::string_view period_key,
    std::vector<RankingEntry> const& entries);
std::string serialize_player_stats_json(PlayerStats const& stats);
std::string serialize_audit_records_json(std::vector<AuditRecord> const& records);
std::string serialize_rollback_summary_json(RollbackSummary const& summary);
std::string serialize_admin_action_log_json(std::vector<AdminActionLogEntry> const& entries);

} // namespace bedrock_insight
