#pragma once

#include "bedrock_insight/core/admin_api.hpp"
#include "bedrock_insight/core/metrics.hpp"
#include "bedrock_insight/core/sqlite_api.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bedrock_insight {

struct AuditFilter final {
    std::optional<std::uint64_t> event_id;
    std::optional<std::int64_t> start_ms;
    std::optional<std::int64_t> end_ms;
    std::string event_kind;
    std::string actor_kind;
    std::string actor_id;
    std::string world_id;
    std::optional<std::int8_t> dimension;
    std::optional<std::int32_t> min_x;
    std::optional<std::int32_t> max_x;
    std::optional<std::int32_t> min_y;
    std::optional<std::int32_t> max_y;
    std::optional<std::int32_t> min_z;
    std::optional<std::int32_t> max_z;
    std::string before_type;
    std::string after_type;
    std::string primary_type;
    std::string secondary_type;
    std::string cause;
    std::size_t limit = 100;
    bool newest_first = true;
};

struct AuditRecord final {
    std::uint64_t event_id = 0;
    std::int64_t occurred_at_ms = 0;
    std::string event_kind;
    std::string actor_kind;
    std::string actor_id;
    std::string actor_name;
    std::string world_id;
    Position position;
    std::string primary_type;
    std::string secondary_type;
    std::string session_id;
    std::string cause;
    std::int64_t amount = 0;
    std::string details_json;
    std::string before_type;
    std::string after_type;
};

struct RollbackSummary final {
    std::string transaction_id;
    std::string operation;
    std::string actor_id;
    std::string reason;
    std::int64_t created_at_ms = 0;
    std::string status;
    std::int64_t entry_count = 0;
    std::int64_t applied_count = 0;
    std::string error;
};

// Read-only query access for dashboard/API workers. A query object never
// shares the writer connection and is not intended for a server tick path.
class AnalyticsQuery final {
public:
    explicit AnalyticsQuery(std::filesystem::path database_path);
    ~AnalyticsQuery();

    AnalyticsQuery(AnalyticsQuery const&) = delete;
    AnalyticsQuery& operator=(AnalyticsQuery const&) = delete;

    [[nodiscard]] bool open(std::string& error);
    void close();

    [[nodiscard]] bool ranking(
        RankingMetric metric,
        RankingPeriod period,
        std::string_view period_key,
        std::size_t limit,
        std::vector<RankingEntry>& result,
        std::string& error) const;

    [[nodiscard]] bool server_metrics(
        std::int64_t start_ms,
        std::int64_t end_ms,
        ServerMetrics& result,
        std::string& error) const;

    [[nodiscard]] bool active_players(
        RankingPeriod period,
        std::string_view period_key,
        std::int64_t& result,
        std::string& error) const;

    [[nodiscard]] bool lookup_audit(
        AuditFilter const& filter,
        std::vector<AuditRecord>& result,
        std::string& error) const;

    [[nodiscard]] bool inspect_audit(
        std::uint64_t event_id,
        AuditRecord& result,
        std::string& error) const;

    [[nodiscard]] bool inspect_rollback(
        std::string_view transaction_id,
        RollbackSummary& result,
        std::string& error) const;

    [[nodiscard]] bool load_api_tokens(std::vector<ApiTokenMetadata>& result, std::string& error) const;

    [[nodiscard]] bool lookup_admin_actions(
        std::size_t limit,
        std::vector<AdminActionLogEntry>& result,
        std::string& error) const;

private:
    [[nodiscard]] bool lookup_audit_locked(
        AuditFilter const& filter,
        std::vector<AuditRecord>& result,
        std::string& error) const;

    std::filesystem::path database_path_;
    mutable std::mutex mutex_;
    mutable sqlite_api::Library sqlite_;
    mutable sqlite_api::sqlite3* db_ = nullptr;
};

} // namespace bedrock_insight
