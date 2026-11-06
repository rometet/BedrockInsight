#include "bedrock_insight/core/analytics_query.hpp"

#include "bedrock_insight/core/sqlite_api.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace bedrock_insight {
namespace {

struct Statement final {
    sqlite_api::Library* library = nullptr;
    sqlite_api::sqlite3_stmt* value = nullptr;

    ~Statement() {
        if (value != nullptr && library != nullptr) {
            library->finalize(value);
        }
    }

    Statement() = default;
    Statement(Statement const&) = delete;
    Statement& operator=(Statement const&) = delete;
};

bool prepare(
    sqlite_api::Library& library,
    sqlite_api::sqlite3* db,
    char const* sql,
    Statement& statement,
    std::string& error) {
    statement.library = &library;
    if (library.prepare_v2(db, sql, -1, &statement.value, nullptr) != sqlite_api::kOk) {
        error = library.errmsg(db);
        return false;
    }
    return true;
}

bool bind_text(
    sqlite_api::Library& library,
    sqlite_api::sqlite3_stmt* statement,
    int index,
    std::string_view value) {
    return library.bind_text(
               statement,
               index,
               value.data(),
               static_cast<int>(value.size()),
               reinterpret_cast<sqlite_api::sqlite3_destructor_type>(-1))
        == sqlite_api::kOk;
}

bool bind_int64(sqlite_api::Library& library, sqlite_api::sqlite3_stmt* statement, int index, std::int64_t value) {
    return library.bind_int64(statement, index, value) == sqlite_api::kOk;
}

char const* metric_column(RankingMetric metric) noexcept {
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
    return nullptr;
}

char const* text_or_empty(sqlite_api::Library& library, sqlite_api::sqlite3_stmt* statement, int column) {
    auto* value = library.column_text(statement, column);
    return value == nullptr ? "" : value;
}

} // namespace

AnalyticsQuery::AnalyticsQuery(std::filesystem::path database_path)
: database_path_(std::move(database_path)) {}

AnalyticsQuery::~AnalyticsQuery() {
    close();
}

bool AnalyticsQuery::open(std::string& error) {
    std::lock_guard lock(mutex_);
    if (db_ != nullptr) {
        return true;
    }
    auto result = sqlite_.open_v2(database_path_.string().c_str(), &db_, sqlite_api::kReadOnly | sqlite_api::kFullMutex, nullptr);
    if (result != sqlite_api::kOk) {
        error = db_ == nullptr ? "sqlite query open failed" : sqlite_.errmsg(db_);
        return false;
    }
    sqlite_.busy_timeout(db_, 5000);
    return true;
}

void AnalyticsQuery::close() {
    std::lock_guard lock(mutex_);
    if (db_ != nullptr) {
        sqlite_.close(db_);
        db_ = nullptr;
    }
}

bool AnalyticsQuery::ranking(
    RankingMetric metric,
    RankingPeriod period,
    std::string_view period_key,
    std::size_t limit,
    std::vector<RankingEntry>& result,
    std::string& error) const {
    result.clear();
    if (limit == 0) {
        return true;
    }

    auto* column = metric_column(metric);
    if (column == nullptr) {
        error = "unknown ranking metric";
        return false;
    }

    std::lock_guard lock(mutex_);
    if (db_ == nullptr) {
        error = "analytics query is not open";
        return false;
    }

    std::string sql;
    if (period == RankingPeriod::Day) {
        sql = "SELECT d.player_id,p.last_name,d." + std::string(column)
            + " FROM player_daily d JOIN players p ON p.player_id=d.player_id WHERE d.day_utc=? AND d."
            + column + " > 0 ORDER BY d." + column + " DESC,d.player_id ASC LIMIT ?;";
    } else if (period == RankingPeriod::Month) {
        sql = "SELECT d.player_id,MAX(p.last_name),SUM(d." + std::string(column)
            + ") FROM player_daily d JOIN players p ON p.player_id=d.player_id WHERE d.day_utc LIKE ? GROUP BY d.player_id HAVING SUM(d."
            + column + ") > 0 ORDER BY SUM(d." + column + ") DESC,d.player_id ASC LIMIT ?;";
    } else {
        sql = "SELECT d.player_id,MAX(p.last_name),SUM(d." + std::string(column)
            + ") FROM player_daily d JOIN players p ON p.player_id=d.player_id GROUP BY d.player_id HAVING SUM(d."
            + column + ") > 0 ORDER BY SUM(d." + column + ") DESC,d.player_id ASC LIMIT ?;";
    }

    Statement statement;
    if (!prepare(sqlite_, db_, sql.c_str(), statement, error)) {
        return false;
    }
    auto bind_index = 1;
    std::string month_pattern;
    if (period == RankingPeriod::Day) {
        if (!bind_text(sqlite_, statement.value, bind_index++, period_key)) {
            error = sqlite_.errmsg(db_);
            return false;
        }
    } else if (period == RankingPeriod::Month) {
        month_pattern = std::string(period_key) + "-%";
        if (!bind_text(sqlite_, statement.value, bind_index++, month_pattern)) {
            error = sqlite_.errmsg(db_);
            return false;
        }
    }
    if (!bind_int64(sqlite_, statement.value, bind_index, static_cast<std::int64_t>(limit))) {
        error = sqlite_.errmsg(db_);
        return false;
    }

    while (true) {
        auto step = sqlite_.step(statement.value);
        if (step == sqlite_api::kDone) {
            break;
        }
        if (step != sqlite_api::kRow) {
            error = sqlite_.errmsg(db_);
            return false;
        }
        result.push_back({
            text_or_empty(sqlite_, statement.value, 0),
            text_or_empty(sqlite_, statement.value, 1),
            sqlite_.column_int64(statement.value, 2),
        });
    }
    return true;
}

bool AnalyticsQuery::server_metrics(
    std::int64_t start_ms,
    std::int64_t end_ms,
    ServerMetrics& result,
    std::string& error) const {
    result = {};
    std::lock_guard lock(mutex_);
    if (db_ == nullptr) {
        error = "analytics query is not open";
        return false;
    }
    Statement statement;
    if (!prepare(
            sqlite_,
            db_,
            "SELECT COUNT(*),COALESCE(SUM(mspt),0),COALESCE(MAX(mspt),0),"
            "COALESCE(MAX(online_players),0),"
            "COALESCE((SELECT online_players FROM server_tick_samples "
            "WHERE occurred_at_ms>=? AND occurred_at_ms<? ORDER BY occurred_at_ms DESC,event_id DESC LIMIT 1),0),"
            "COALESCE((SELECT occurred_at_ms FROM server_tick_samples "
            "WHERE occurred_at_ms>=? AND occurred_at_ms<? ORDER BY occurred_at_ms DESC,event_id DESC LIMIT 1),0) "
            "FROM server_tick_samples WHERE occurred_at_ms>=? AND occurred_at_ms<?;",
            statement,
            error)) {
        return false;
    }
    if (!bind_int64(sqlite_, statement.value, 1, start_ms)
        || !bind_int64(sqlite_, statement.value, 2, end_ms)
        || !bind_int64(sqlite_, statement.value, 3, start_ms)
        || !bind_int64(sqlite_, statement.value, 4, end_ms)
        || !bind_int64(sqlite_, statement.value, 5, start_ms)
        || !bind_int64(sqlite_, statement.value, 6, end_ms)) {
        error = sqlite_.errmsg(db_);
        return false;
    }
    if (sqlite_.step(statement.value) != sqlite_api::kRow) {
        error = sqlite_.errmsg(db_);
        return false;
    }
    result.tick_samples = static_cast<std::uint64_t>(sqlite_.column_int64(statement.value, 0));
    result.total_mspt = sqlite_.column_int64(statement.value, 1);
    result.max_mspt = sqlite_.column_int64(statement.value, 2);
    result.peak_online = sqlite_.column_int(statement.value, 3);
    result.current_online = sqlite_.column_int(statement.value, 4);
    result.last_sample_ms = sqlite_.column_int64(statement.value, 5);
    return true;
}

bool AnalyticsQuery::active_players(
    RankingPeriod period,
    std::string_view period_key,
    std::int64_t& result,
    std::string& error) const {
    result = 0;
    std::lock_guard lock(mutex_);
    if (db_ == nullptr) {
        error = "analytics query is not open";
        return false;
    }

    char const* sql = nullptr;
    if (period == RankingPeriod::Day) {
        sql = "SELECT COUNT(DISTINCT actor_id) FROM events WHERE actor_kind='player' "
              "AND date(occurred_at_ms / 1000,'unixepoch')=?;";
    } else if (period == RankingPeriod::Month) {
        sql = "SELECT COUNT(DISTINCT actor_id) FROM events WHERE actor_kind='player' "
              "AND strftime('%Y-%m',occurred_at_ms / 1000,'unixepoch')=?;";
    } else {
        sql = "SELECT COUNT(DISTINCT actor_id) FROM events WHERE actor_kind='player';";
    }

    Statement statement;
    if (!prepare(sqlite_, db_, sql, statement, error)) {
        return false;
    }
    if (period != RankingPeriod::AllTime && !bind_text(sqlite_, statement.value, 1, period_key)) {
        error = sqlite_.errmsg(db_);
        return false;
    }
    if (sqlite_.step(statement.value) != sqlite_api::kRow) {
        error = sqlite_.errmsg(db_);
        return false;
    }
    result = sqlite_.column_int64(statement.value, 0);
    return true;
}

bool AnalyticsQuery::lookup_audit(
    AuditFilter const& filter,
    std::vector<AuditRecord>& result,
    std::string& error) const {
    result.clear();
    if (filter.limit == 0) {
        return true;
    }
    std::lock_guard lock(mutex_);
    return lookup_audit_locked(filter, result, error);
}

bool AnalyticsQuery::inspect_audit(
    std::uint64_t event_id,
    AuditRecord& result,
    std::string& error) const {
    std::lock_guard lock(mutex_);
    AuditFilter filter;
    filter.event_id = event_id;
    filter.limit = 1;
    filter.newest_first = false;
    std::vector<AuditRecord> records;
    if (!lookup_audit_locked(filter, records, error)) {
        return false;
    }
    if (records.empty()) {
        error = "audit event not found";
        return false;
    }
    result = std::move(records.front());
    return true;
}

bool AnalyticsQuery::lookup_audit_locked(
    AuditFilter const& filter,
    std::vector<AuditRecord>& result,
    std::string& error) const {
    if (db_ == nullptr) {
        error = "analytics query is not open";
        return false;
    }

    struct BindValue final {
        bool text = false;
        std::string text_value;
        std::int64_t integer_value = 0;
    };

    std::string sql =
        "SELECT e.event_id,e.occurred_at_ms,a.action,e.actor_kind,e.actor_id,e.actor_name,e.world_id,"
        "e.dimension,e.x,e.y,e.z,e.primary_type,e.secondary_type,e.session_id,e.cause,e.amount,e.details_json,"
        "a.before_type,a.after_type FROM audit_events a JOIN events e ON e.event_id=a.event_id WHERE 1=1";
    std::vector<BindValue> binds;
    auto add_text = [&](char const* clause, std::string const& value) {
        sql += clause;
        binds.push_back({true, value, 0});
    };
    auto add_integer = [&](char const* clause, std::int64_t value) {
        sql += clause;
        binds.push_back({false, {}, value});
    };

    if (filter.event_id.has_value()) {
        add_integer(" AND e.event_id=?", static_cast<std::int64_t>(*filter.event_id));
    }
    if (filter.start_ms.has_value()) {
        add_integer(" AND e.occurred_at_ms>=?", *filter.start_ms);
    }
    if (filter.end_ms.has_value()) {
        add_integer(" AND e.occurred_at_ms<?", *filter.end_ms);
    }
    if (!filter.event_kind.empty()) {
        add_text(" AND a.action=?", filter.event_kind);
    }
    if (!filter.actor_kind.empty()) {
        add_text(" AND e.actor_kind=?", filter.actor_kind);
    }
    if (!filter.actor_id.empty()) {
        add_text(" AND e.actor_id=?", filter.actor_id);
    }
    if (!filter.world_id.empty()) {
        add_text(" AND e.world_id=?", filter.world_id);
    }
    if (filter.dimension.has_value()) {
        add_integer(" AND e.dimension=?", *filter.dimension);
    }
    if (filter.min_x.has_value()) {
        add_integer(" AND e.x>=?", *filter.min_x);
    }
    if (filter.max_x.has_value()) {
        add_integer(" AND e.x<=?", *filter.max_x);
    }
    if (filter.min_y.has_value()) {
        add_integer(" AND e.y>=?", *filter.min_y);
    }
    if (filter.max_y.has_value()) {
        add_integer(" AND e.y<=?", *filter.max_y);
    }
    if (filter.min_z.has_value()) {
        add_integer(" AND e.z>=?", *filter.min_z);
    }
    if (filter.max_z.has_value()) {
        add_integer(" AND e.z<=?", *filter.max_z);
    }
    if (!filter.before_type.empty()) {
        add_text(" AND a.before_type=?", filter.before_type);
    }
    if (!filter.after_type.empty()) {
        add_text(" AND a.after_type=?", filter.after_type);
    }
    if (!filter.primary_type.empty()) {
        add_text(" AND e.primary_type=?", filter.primary_type);
    }
    if (!filter.secondary_type.empty()) {
        add_text(" AND e.secondary_type=?", filter.secondary_type);
    }
    if (!filter.cause.empty()) {
        add_text(" AND e.cause=?", filter.cause);
    }

    sql += filter.newest_first ? " ORDER BY e.occurred_at_ms DESC,e.event_id DESC LIMIT ?;"
                               : " ORDER BY e.occurred_at_ms ASC,e.event_id ASC LIMIT ?;";

    Statement statement;
    if (!prepare(sqlite_, db_, sql.c_str(), statement, error)) {
        return false;
    }
    auto index = 1;
    for (auto const& bind : binds) {
        auto bound = bind.text
            ? bind_text(sqlite_, statement.value, index, bind.text_value)
            : bind_int64(sqlite_, statement.value, index, bind.integer_value);
        if (!bound) {
            error = sqlite_.errmsg(db_);
            return false;
        }
        ++index;
    }
    auto limit = filter.limit > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())
        ? std::numeric_limits<std::int64_t>::max()
        : static_cast<std::int64_t>(filter.limit);
    if (!bind_int64(sqlite_, statement.value, index, limit)) {
        error = sqlite_.errmsg(db_);
        return false;
    }

    while (true) {
        auto step = sqlite_.step(statement.value);
        if (step == sqlite_api::kDone) {
            break;
        }
        if (step != sqlite_api::kRow) {
            error = sqlite_.errmsg(db_);
            return false;
        }
        AuditRecord record;
        record.event_id = static_cast<std::uint64_t>(sqlite_.column_int64(statement.value, 0));
        record.occurred_at_ms = sqlite_.column_int64(statement.value, 1);
        record.event_kind = text_or_empty(sqlite_, statement.value, 2);
        record.actor_kind = text_or_empty(sqlite_, statement.value, 3);
        record.actor_id = text_or_empty(sqlite_, statement.value, 4);
        record.actor_name = text_or_empty(sqlite_, statement.value, 5);
        record.world_id = text_or_empty(sqlite_, statement.value, 6);
        record.position.dimension = static_cast<std::int8_t>(sqlite_.column_int(statement.value, 7));
        record.position.x = sqlite_.column_int(statement.value, 8);
        record.position.y = sqlite_.column_int(statement.value, 9);
        record.position.z = sqlite_.column_int(statement.value, 10);
        record.primary_type = text_or_empty(sqlite_, statement.value, 11);
        record.secondary_type = text_or_empty(sqlite_, statement.value, 12);
        record.session_id = text_or_empty(sqlite_, statement.value, 13);
        record.cause = text_or_empty(sqlite_, statement.value, 14);
        record.amount = sqlite_.column_int64(statement.value, 15);
        record.details_json = text_or_empty(sqlite_, statement.value, 16);
        record.before_type = text_or_empty(sqlite_, statement.value, 17);
        record.after_type = text_or_empty(sqlite_, statement.value, 18);
        result.push_back(std::move(record));
    }
    return true;
}

bool AnalyticsQuery::inspect_rollback(
    std::string_view transaction_id,
    RollbackSummary& result,
    std::string& error) const {
    result = {};
    std::lock_guard lock(mutex_);
    if (db_ == nullptr) {
        error = "analytics query is not open";
        return false;
    }
    Statement statement;
    if (!prepare(
            sqlite_,
            db_,
            "SELECT transaction_id,operation,actor_id,reason,created_at_ms,status,entry_count,applied_count,error "
            "FROM rollback_transactions WHERE transaction_id=?;",
            statement,
            error)
        || !bind_text(sqlite_, statement.value, 1, transaction_id)) {
        error = sqlite_.errmsg(db_);
        return false;
    }
    auto step = sqlite_.step(statement.value);
    if (step == sqlite_api::kDone) {
        error = "rollback transaction not found";
        return false;
    }
    if (step != sqlite_api::kRow) {
        error = sqlite_.errmsg(db_);
        return false;
    }
    result.transaction_id = text_or_empty(sqlite_, statement.value, 0);
    result.operation = text_or_empty(sqlite_, statement.value, 1);
    result.actor_id = text_or_empty(sqlite_, statement.value, 2);
    result.reason = text_or_empty(sqlite_, statement.value, 3);
    result.created_at_ms = sqlite_.column_int64(statement.value, 4);
    result.status = text_or_empty(sqlite_, statement.value, 5);
    result.entry_count = sqlite_.column_int64(statement.value, 6);
    result.applied_count = sqlite_.column_int64(statement.value, 7);
    result.error = text_or_empty(sqlite_, statement.value, 8);
    return true;
}

bool AnalyticsQuery::load_api_tokens(std::vector<ApiTokenMetadata>& result, std::string& error) const {
    result.clear();
    std::lock_guard lock(mutex_);
    if (db_ == nullptr) {
        error = "analytics query is not open";
        return false;
    }
    Statement statement;
    if (!prepare(
            sqlite_,
            db_,
            "SELECT token_id,secret_hash,actor_id,role,created_at_ms,enabled FROM api_tokens ORDER BY token_id;",
            statement,
            error)) {
        return false;
    }
    for (;;) {
        auto step = sqlite_.step(statement.value);
        if (step == sqlite_api::kDone) {
            break;
        }
        if (step != sqlite_api::kRow) {
            error = sqlite_.errmsg(db_);
            return false;
        }
        auto role = api_role_from_string(text_or_empty(sqlite_, statement.value, 3));
        if (!role.has_value()) {
            error = "unknown API role in database";
            return false;
        }
        result.push_back(ApiTokenMetadata{
            text_or_empty(sqlite_, statement.value, 0),
            text_or_empty(sqlite_, statement.value, 1),
            text_or_empty(sqlite_, statement.value, 2),
            *role,
            sqlite_.column_int64(statement.value, 4),
            sqlite_.column_int(statement.value, 5) != 0,
        });
    }
    return true;
}

bool AnalyticsQuery::lookup_admin_actions(
    std::size_t limit,
    std::vector<AdminActionLogEntry>& result,
    std::string& error) const {
    result.clear();
    if (limit == 0) {
        return true;
    }
    auto bounded_limit = static_cast<std::int64_t>(std::min<std::size_t>(
        limit, static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())));

    std::lock_guard lock(mutex_);
    if (db_ == nullptr) {
        error = "analytics query is not open";
        return false;
    }
    Statement statement;
    if (!prepare(
            sqlite_,
            db_,
            "SELECT id,occurred_at_ms,actor_id,role,action,method,path,remote_address,allowed,status,detail "
            "FROM admin_action_log ORDER BY id DESC LIMIT ?;",
            statement,
            error)
        || !bind_int64(sqlite_, statement.value, 1, bounded_limit)) {
        error = sqlite_.errmsg(db_);
        return false;
    }
    for (;;) {
        auto step = sqlite_.step(statement.value);
        if (step == sqlite_api::kDone) {
            break;
        }
        if (step != sqlite_api::kRow) {
            error = sqlite_.errmsg(db_);
            return false;
        }
        auto role = api_role_from_string(text_or_empty(sqlite_, statement.value, 3));
        if (!role.has_value()) {
            error = "unknown API role in database";
            return false;
        }
        result.push_back(AdminActionLogEntry{
            static_cast<std::uint64_t>(sqlite_.column_int64(statement.value, 0)),
            sqlite_.column_int64(statement.value, 1),
            text_or_empty(sqlite_, statement.value, 2),
            *role,
            text_or_empty(sqlite_, statement.value, 4),
            text_or_empty(sqlite_, statement.value, 5),
            text_or_empty(sqlite_, statement.value, 6),
            text_or_empty(sqlite_, statement.value, 7),
            sqlite_.column_int(statement.value, 8) != 0,
            sqlite_.column_int(statement.value, 9),
            text_or_empty(sqlite_, statement.value, 10),
        });
    }
    return true;
}

} // namespace bedrock_insight
