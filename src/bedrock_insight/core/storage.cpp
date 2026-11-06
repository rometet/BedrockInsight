#include "bedrock_insight/core/storage.hpp"

#include "bedrock_insight/core/schema.hpp"
#include "bedrock_insight/core/sqlite_api.hpp"

#include <chrono>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace bedrock_insight {
namespace {

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

struct Statement final {
    sqlite_api::Library* library = nullptr;
    sqlite_api::sqlite3_stmt* value = nullptr;

    Statement() = default;

    ~Statement() {
        if (value != nullptr && library != nullptr) {
            library->finalize(value);
        }
    }

    Statement(Statement const&) = delete;
    Statement& operator=(Statement const&) = delete;
};

bool bind_text(sqlite_api::Library& library, sqlite_api::sqlite3_stmt* statement, int index, std::string const& value) {
    return library.bind_text(statement, index, value.data(), static_cast<int>(value.size()), reinterpret_cast<sqlite_api::sqlite3_destructor_type>(-1))
        == sqlite_api::kOk;
}

bool bind_int64(sqlite_api::Library& library, sqlite_api::sqlite3_stmt* statement, int index, std::int64_t value) {
    return library.bind_int64(statement, index, value) == sqlite_api::kOk;
}

bool prepare(sqlite_api::Library& library, sqlite_api::sqlite3* db, char const* sql, Statement& statement, std::string& error) {
    statement.library = &library;
    auto result = library.prepare_v2(db, sql, -1, &statement.value, nullptr);
    if (result != sqlite_api::kOk) {
        error = library.errmsg(db);
        return false;
    }
    return true;
}

bool step_done(sqlite_api::Library& library, sqlite_api::sqlite3* db, Statement& statement, std::string& error) {
    auto result = library.step(statement.value);
    if (result != sqlite_api::kDone) {
        error = library.errmsg(db);
        return false;
    }
    return true;
}

bool is_audit(EventKind kind) {
    switch (kind) {
    case EventKind::AuditBlockChange:
    case EventKind::AuditContainerChange:
    case EventKind::AuditFluidChange:
    case EventKind::AuditPiston:
    case EventKind::AuditExplosion:
    case EventKind::AuditEntityChange:
        return true;
    default:
        return false;
    }
}

char const* type_metric(EventKind kind) {
    switch (kind) {
    case EventKind::BlockBreak: return "blocks_broken";
    case EventKind::BlockPlace: return "blocks_placed";
    case EventKind::ItemCraft: return "items_crafted";
    case EventKind::ItemUse: return "items_used";
    case EventKind::EntityKill: return "kills";
    case EventKind::ItemCraftResult: return "items_crafted";
    case EventKind::PlayerTravelDistance: return "travel_cm";
    case EventKind::PlayerInteractionResult: return "interactions";
    case EventKind::PlayerDamageTaken: return "damage_taken_tenths";
    case EventKind::PlayerDamageDealt: return "damage_dealt_tenths";
    case EventKind::PlayerSleepResult: return "sleep_successes";
    case EventKind::PlayerSneakTime: return "sneak_time_ticks";
    case EventKind::ItemBrokenResult: return "broken_items";
    case EventKind::PlayerKilledByEntity: return "killed_by_entities";
    default: return nullptr;
    }
}

std::int64_t positive_amount(std::int64_t amount) {
    return amount > 0 ? amount : 0;
}

std::string source_event_ids_json(std::vector<std::uint64_t> const& event_ids) {
    std::string result = "[";
    for (std::size_t index = 0; index < event_ids.size(); ++index) {
        if (index != 0) {
            result += ',';
        }
        result += std::to_string(event_ids[index]);
    }
    result += ']';
    return result;
}

std::int64_t applied_entry_count(RollbackTransaction const& transaction) {
    std::int64_t count = 0;
    for (auto const& entry : transaction.entries) {
        if (entry.status == RollbackEntryStatus::Applied) {
            ++count;
        }
    }
    return count;
}

std::string transaction_error(RollbackTransaction const& transaction) {
    for (auto const& entry : transaction.entries) {
        if (!entry.error.empty()) {
            return entry.error;
        }
    }
    return {};
}

} // namespace

class StorageWriter::Database final {
public:
    ~Database() {
        if (db_ != nullptr) {
            sqlite_.close(db_);
        }
    }

    bool open(std::filesystem::path const& path, std::string& error) {
        auto parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code filesystem_error;
            std::filesystem::create_directories(parent, filesystem_error);
            if (filesystem_error) {
                error = "could not create database directory: " + filesystem_error.message();
                return false;
            }
        }

        auto result = sqlite_.open_v2(
            path.string().c_str(),
            &db_,
            sqlite_api::kReadWrite | sqlite_api::kCreate | sqlite_api::kFullMutex,
            nullptr);
        if (result != sqlite_api::kOk) {
            error = db_ == nullptr ? "sqlite open failed" : sqlite_.errmsg(db_);
            return false;
        }

        sqlite_.busy_timeout(db_, 5000);
        if (!exec("PRAGMA journal_mode=WAL;", error)
            || !exec("PRAGMA synchronous=NORMAL;", error)
            || !exec("PRAGMA foreign_keys=ON;", error)
            || !migrate(error)) {
            return false;
        }
        return true;
    }

    bool last_sequence(std::uint64_t& sequence, std::string& error) {
        Statement statement;
        if (!prepare(sqlite_, db_, "SELECT COALESCE(MAX(event_id),0) FROM events;", statement, error)
            || sqlite_.step(statement.value) != sqlite_api::kRow) {
            if (error.empty()) error = sqlite_.errmsg(db_);
            return false;
        }
        auto value = sqlite_.column_int64(statement.value, 0);
        if (value < 0 || value == std::numeric_limits<std::int64_t>::max()) {
            error = "durable event ID range exhausted or invalid";
            return false;
        }
        sequence = static_cast<std::uint64_t>(value);
        return true;
    }

    bool insert(AnalyticsEvent const& event, std::string& error) {
        if (!exec("BEGIN IMMEDIATE;", error)) {
            return false;
        }

        if (!upsert_player(event, error) || !insert_event(event, error) || !update_daily(event, error)
            || !update_daily_type(event, error) || !update_server_sample(event, error)
            || !update_session(event, error) || !insert_audit(event, error)) {
            exec("ROLLBACK;", error);
            return false;
        }

        if (!exec("COMMIT;", error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        return true;
    }

    bool insert_rollback_transaction(RollbackTransaction const& transaction, std::string& error) {
        if (!exec("BEGIN IMMEDIATE;", error)) {
            return false;
        }

        Statement transaction_statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO rollback_transactions(transaction_id,operation,actor_id,reason,created_at_ms,status,entry_count,applied_count,error) "
                "VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(transaction_id) DO UPDATE SET "
                "operation=excluded.operation,actor_id=excluded.actor_id,reason=excluded.reason,created_at_ms=excluded.created_at_ms,"
                "status=excluded.status,entry_count=excluded.entry_count,applied_count=excluded.applied_count,error=excluded.error;",
                transaction_statement,
                error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        auto transaction_error_text = transaction_error(transaction);
        if (!bind_text(sqlite_, transaction_statement.value, 1, transaction.transaction_id)
            || !bind_text(sqlite_, transaction_statement.value, 2, std::string(to_string(transaction.operation)))
            || !bind_text(sqlite_, transaction_statement.value, 3, transaction.actor_id)
            || !bind_text(sqlite_, transaction_statement.value, 4, transaction.reason)
            || !bind_int64(sqlite_, transaction_statement.value, 5, transaction.created_at_ms)
            || !bind_text(sqlite_, transaction_statement.value, 6, std::string(to_string(transaction.status)))
            || !bind_int64(sqlite_, transaction_statement.value, 7, static_cast<std::int64_t>(transaction.entries.size()))
            || !bind_int64(sqlite_, transaction_statement.value, 8, applied_entry_count(transaction))
            || !bind_text(sqlite_, transaction_statement.value, 9, transaction_error_text)
            || !step_done(sqlite_, db_, transaction_statement, error)) {
            exec("ROLLBACK;", error);
            return false;
        }

        Statement delete_entries;
        if (!prepare(
                sqlite_,
                db_,
                "DELETE FROM rollback_entries WHERE transaction_id=?;",
                delete_entries,
                error)
            || !bind_text(sqlite_, delete_entries.value, 1, transaction.transaction_id)
            || !step_done(sqlite_, db_, delete_entries, error)) {
            exec("ROLLBACK;", error);
            return false;
        }

        for (std::size_t index = 0; index < transaction.entries.size(); ++index) {
            auto const& entry = transaction.entries[index];
            Statement entry_statement;
            if (!prepare(
                    sqlite_,
                    db_,
                    "INSERT INTO rollback_entries(transaction_id,entry_index,event_id,source_event_ids_json,world_id,dimension,x,y,z,"
                    "expected_type,target_type,observed_type,status,error) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                    entry_statement,
                    error)
                || !bind_text(sqlite_, entry_statement.value, 1, transaction.transaction_id)
                || !bind_int64(sqlite_, entry_statement.value, 2, static_cast<std::int64_t>(index))
                || !bind_int64(sqlite_, entry_statement.value, 3, static_cast<std::int64_t>(entry.event_id))
                || !bind_text(sqlite_, entry_statement.value, 4, source_event_ids_json(transaction.entries[index].source_event_ids))
                || !bind_text(sqlite_, entry_statement.value, 5, entry.world_id)
                || !bind_int64(sqlite_, entry_statement.value, 6, entry.position.dimension)
                || !bind_int64(sqlite_, entry_statement.value, 7, entry.position.x)
                || !bind_int64(sqlite_, entry_statement.value, 8, entry.position.y)
                || !bind_int64(sqlite_, entry_statement.value, 9, entry.position.z)
                || !bind_text(sqlite_, entry_statement.value, 10, entry.expected_type)
                || !bind_text(sqlite_, entry_statement.value, 11, entry.target_type)
                || !bind_text(sqlite_, entry_statement.value, 12, entry.observed_type)
                || !bind_text(sqlite_, entry_statement.value, 13, std::string(to_string(entry.status)))
                || !bind_text(sqlite_, entry_statement.value, 14, entry.error)
                || !step_done(sqlite_, db_, entry_statement, error)) {
                exec("ROLLBACK;", error);
                return false;
            }
        }

        if (!exec("COMMIT;", error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        return true;
    }

    bool insert_api_token(ApiTokenMetadata const& token, std::string& error) {
        if (!exec("BEGIN IMMEDIATE;", error)) {
            return false;
        }
        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO api_tokens(token_id,secret_hash,actor_id,role,created_at_ms,enabled) VALUES(?,?,?,?,?,?) "
                "ON CONFLICT(token_id) DO UPDATE SET secret_hash=excluded.secret_hash,actor_id=excluded.actor_id,"
                "role=excluded.role,created_at_ms=excluded.created_at_ms,enabled=excluded.enabled;",
                statement,
                error)
            || !bind_text(sqlite_, statement.value, 1, token.token_id)
            || !bind_text(sqlite_, statement.value, 2, token.secret_hash)
            || !bind_text(sqlite_, statement.value, 3, token.actor_id)
            || !bind_text(sqlite_, statement.value, 4, std::string(to_string(token.role)))
            || !bind_int64(sqlite_, statement.value, 5, token.created_at_ms)
            || !bind_int64(sqlite_, statement.value, 6, token.enabled ? 1 : 0)
            || !step_done(sqlite_, db_, statement, error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        if (!exec("COMMIT;", error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        return true;
    }

    bool insert_admin_action(AdminActionLogEntry const& entry, std::string& error) {
        if (!exec("BEGIN IMMEDIATE;", error)) {
            return false;
        }
        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO admin_action_log(occurred_at_ms,actor_id,role,action,method,path,remote_address,allowed,status,detail) "
                "VALUES(?,?,?,?,?,?,?,?,?,?);",
                statement,
                error)
            || !bind_int64(sqlite_, statement.value, 1, entry.occurred_at_ms)
            || !bind_text(sqlite_, statement.value, 2, entry.actor_id)
            || !bind_text(sqlite_, statement.value, 3, std::string(to_string(entry.role)))
            || !bind_text(sqlite_, statement.value, 4, entry.action)
            || !bind_text(sqlite_, statement.value, 5, entry.method)
            || !bind_text(sqlite_, statement.value, 6, entry.path)
            || !bind_text(sqlite_, statement.value, 7, entry.remote_address)
            || !bind_int64(sqlite_, statement.value, 8, entry.allowed ? 1 : 0)
            || !bind_int64(sqlite_, statement.value, 9, entry.status)
            || !bind_text(sqlite_, statement.value, 10, entry.detail)
            || !step_done(sqlite_, db_, statement, error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        if (!exec("COMMIT;", error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        return true;
    }

private:
    bool exec(char const* sql, std::string& error) {
        char* sqlite_error = nullptr;
        auto result = sqlite_.exec(db_, sql, nullptr, nullptr, &sqlite_error);
        if (result != sqlite_api::kOk) {
            error = sqlite_error == nullptr ? sqlite_.errmsg(db_) : sqlite_error;
            sqlite_.free(sqlite_error);
            return false;
        }
        return true;
    }

    bool migrate(std::string& error) {
        Statement version_statement;
        if (!prepare(sqlite_, db_, "PRAGMA user_version;", version_statement, error)) {
            return false;
        }
        auto result = sqlite_.step(version_statement.value);
        if (result != sqlite_api::kRow) {
            error = sqlite_.errmsg(db_);
            return false;
        }
        auto current_version = sqlite_.column_int(version_statement.value, 0);
        if (current_version > schema::kVersion) {
            error = "database schema is newer than this mod";
            return false;
        }
        if (current_version == schema::kVersion) {
            return true;
        }

        if (!exec("BEGIN IMMEDIATE;", error)) {
            return false;
        }

        auto apply_migration = [&](int version, std::string_view sql) {
            if (!exec(sql.data(), error)) {
                return false;
            }
            auto pragma = "PRAGMA user_version=" + std::to_string(version) + ";";
            if (!exec(pragma.c_str(), error)) {
                return false;
            }
            Statement metadata;
            return prepare(
                       sqlite_,
                       db_,
                       "INSERT OR REPLACE INTO schema_meta(version, applied_at_ms) VALUES(?, ?);",
                       metadata,
                       error)
                && bind_int64(sqlite_, metadata.value, 1, version)
                && bind_int64(sqlite_, metadata.value, 2, now_ms())
                && step_done(sqlite_, db_, metadata, error);
        };

        if (current_version < 1 && !apply_migration(1, schema::kMigrationV1)) {
            exec("ROLLBACK;", error);
            return false;
        }
        if (current_version < 2 && !apply_migration(2, schema::kMigrationV2)) {
            exec("ROLLBACK;", error);
            return false;
        }
        if (current_version < 3 && !apply_migration(3, schema::kMigrationV3)) {
            exec("ROLLBACK;", error);
            return false;
        }
        if (current_version < 4 && !apply_migration(4, schema::kMigrationV4)) {
            exec("ROLLBACK;", error);
            return false;
        }
        if (!exec("COMMIT;", error)) {
            exec("ROLLBACK;", error);
            return false;
        }
        return true;
    }

    bool upsert_player(AnalyticsEvent const& event, std::string& error) {
        if (!event.has_player_actor()) {
            return true;
        }
        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO players(player_id,last_name,first_seen_ms,last_seen_ms) VALUES(?,?,?,?) "
                "ON CONFLICT(player_id) DO UPDATE SET last_name=excluded.last_name,last_seen_ms=excluded.last_seen_ms;",
                statement,
                error)) {
            return false;
        }
        return bind_text(sqlite_, statement.value, 1, event.actor.id)
            && bind_text(sqlite_, statement.value, 2, event.actor.display_name)
            && bind_int64(sqlite_, statement.value, 3, event.occurred_at_ms)
            && bind_int64(sqlite_, statement.value, 4, event.occurred_at_ms) && step_done(sqlite_, db_, statement, error);
    }

    bool insert_event(AnalyticsEvent const& event, std::string& error) {
        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO events(event_id,occurred_at_ms,event_kind,actor_kind,actor_id,actor_name,world_id,"
                "dimension,x,y,z,primary_type,secondary_type,session_id,cause,amount,details_json,tick_mspt,online_players) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                statement,
                error)) {
            return false;
        }
        return bind_int64(sqlite_, statement.value, 1, static_cast<std::int64_t>(event.sequence))
            && bind_int64(sqlite_, statement.value, 2, event.occurred_at_ms)
            && bind_text(sqlite_, statement.value, 3, std::string(to_string(event.kind)))
            && bind_text(sqlite_, statement.value, 4, std::string(to_string(event.actor.kind)))
            && bind_text(sqlite_, statement.value, 5, event.actor.id)
            && bind_text(sqlite_, statement.value, 6, event.actor.display_name)
            && bind_text(sqlite_, statement.value, 7, event.world_id)
            && bind_int64(sqlite_, statement.value, 8, event.position.dimension)
            && bind_int64(sqlite_, statement.value, 9, event.position.x)
            && bind_int64(sqlite_, statement.value, 10, event.position.y)
            && bind_int64(sqlite_, statement.value, 11, event.position.z)
            && bind_text(sqlite_, statement.value, 12, event.primary_type)
            && bind_text(sqlite_, statement.value, 13, event.secondary_type)
            && bind_text(sqlite_, statement.value, 14, event.session_id)
            && bind_text(sqlite_, statement.value, 15, event.cause)
            && bind_int64(sqlite_, statement.value, 16, event.amount)
            && bind_text(sqlite_, statement.value, 17, event.details_json)
            && bind_int64(sqlite_, statement.value, 18, event.tick_mspt)
            && bind_int64(sqlite_, statement.value, 19, event.online_players)
            && step_done(sqlite_, db_, statement, error);
    }

    bool update_daily(AnalyticsEvent const& event, std::string& error) {
        if (!event.has_player_actor()) {
            return true;
        }

        std::int64_t blocks_broken = 0;
        std::int64_t blocks_placed = 0;
        std::int64_t items_crafted = 0;
        std::int64_t items_used = 0;
        std::int64_t kills = 0;
        std::int64_t deaths = 0;
        std::int64_t distance = 0;
        std::int64_t sessions = 0;
        switch (event.kind) {
        case EventKind::BlockBreak: blocks_broken = positive_amount(event.amount); break;
        case EventKind::BlockPlace: blocks_placed = positive_amount(event.amount); break;
        case EventKind::ItemCraft: items_crafted = positive_amount(event.amount); break;
        case EventKind::ItemUse: items_used = positive_amount(event.amount); break;
        case EventKind::EntityKill: kills = positive_amount(event.amount); break;
        case EventKind::PlayerDeath: deaths = positive_amount(event.amount); break;
        case EventKind::Distance: distance = positive_amount(event.amount); break;
        case EventKind::ItemCraftResult: items_crafted = positive_amount(event.amount); break;
        // Movement events are stored in centimeters; the legacy daily column is
        // whole meters, so the remainder is kept in player_daily_types only.
        case EventKind::PlayerTravelDistance: distance = positive_amount(event.amount) / 100; break;
        case EventKind::SessionStart: sessions = 1; break;
        default: return true;
        }

        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO player_daily(player_id,day_utc,blocks_broken,blocks_placed,items_crafted,items_used,"
                "kills,deaths,distance_m,sessions) VALUES(?,date(? / 1000,'unixepoch'),?,?,?,?,?,?,?,?) "
                "ON CONFLICT(player_id,day_utc) DO UPDATE SET "
                "blocks_broken=blocks_broken+excluded.blocks_broken,"
                "blocks_placed=blocks_placed+excluded.blocks_placed,"
                "items_crafted=items_crafted+excluded.items_crafted,"
                "items_used=items_used+excluded.items_used,kills=kills+excluded.kills,deaths=deaths+excluded.deaths,"
                "distance_m=distance_m+excluded.distance_m,sessions=sessions+excluded.sessions;",
                statement,
                error)) {
            return false;
        }
        return bind_text(sqlite_, statement.value, 1, event.actor.id)
            && bind_int64(sqlite_, statement.value, 2, event.occurred_at_ms)
            && bind_int64(sqlite_, statement.value, 3, blocks_broken)
            && bind_int64(sqlite_, statement.value, 4, blocks_placed)
            && bind_int64(sqlite_, statement.value, 5, items_crafted)
            && bind_int64(sqlite_, statement.value, 6, items_used)
            && bind_int64(sqlite_, statement.value, 7, kills)
            && bind_int64(sqlite_, statement.value, 8, deaths)
            && bind_int64(sqlite_, statement.value, 9, distance)
            && bind_int64(sqlite_, statement.value, 10, sessions) && step_done(sqlite_, db_, statement, error);
    }

    bool update_daily_type(AnalyticsEvent const& event, std::string& error) {
        auto metric = type_metric(event.kind);
        if (!event.has_player_actor() || metric == nullptr || event.primary_type.empty()) {
            return true;
        }
        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO player_daily_types(player_id,day_utc,metric,value_type,amount) "
                "VALUES(?,date(? / 1000,'unixepoch'),?,?,?) "
                "ON CONFLICT(player_id,day_utc,metric,value_type) DO UPDATE SET amount=amount+excluded.amount;",
                statement,
                error)) {
            return false;
        }
        return bind_text(sqlite_, statement.value, 1, event.actor.id)
            && bind_int64(sqlite_, statement.value, 2, event.occurred_at_ms)
            && bind_text(sqlite_, statement.value, 3, metric)
            && bind_text(sqlite_, statement.value, 4, event.primary_type)
            && bind_int64(sqlite_, statement.value, 5, positive_amount(event.amount))
            && step_done(sqlite_, db_, statement, error);
    }

    bool update_server_sample(AnalyticsEvent const& event, std::string& error) {
        if (event.kind != EventKind::ServerTick || event.tick_mspt <= 0 || event.online_players < 0) {
            return true;
        }
        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT OR REPLACE INTO server_tick_samples(event_id,occurred_at_ms,mspt,online_players) "
                "VALUES(?,?,?,?);",
                statement,
                error)) {
            return false;
        }
        return bind_int64(sqlite_, statement.value, 1, static_cast<std::int64_t>(event.sequence))
            && bind_int64(sqlite_, statement.value, 2, event.occurred_at_ms)
            && bind_int64(sqlite_, statement.value, 3, event.tick_mspt)
            && bind_int64(sqlite_, statement.value, 4, event.online_players)
            && step_done(sqlite_, db_, statement, error);
    }

    bool update_session(AnalyticsEvent const& event, std::string& error) {
        if (!event.has_player_actor() || event.session_id.empty()) {
            return true;
        }

        if (event.kind == EventKind::SessionStart) {
            Statement statement;
            if (!prepare(
                    sqlite_,
                    db_,
                    "INSERT OR IGNORE INTO sessions(session_id,player_id,started_at_ms) VALUES(?,?,?);",
                    statement,
                    error)) {
                return false;
            }
            return bind_text(sqlite_, statement.value, 1, event.session_id)
                && bind_text(sqlite_, statement.value, 2, event.actor.id)
                && bind_int64(sqlite_, statement.value, 3, event.occurred_at_ms)
                && step_done(sqlite_, db_, statement, error);
        }

        if (event.kind == EventKind::SessionEnd) {
            Statement statement;
            if (!prepare(
                    sqlite_,
                    db_,
                    "UPDATE sessions SET ended_at_ms=?,duration_ms=MAX(0,? - started_at_ms) WHERE session_id=?;",
                    statement,
                    error)) {
                return false;
            }
            return bind_int64(sqlite_, statement.value, 1, event.occurred_at_ms)
                && bind_int64(sqlite_, statement.value, 2, event.occurred_at_ms)
                && bind_text(sqlite_, statement.value, 3, event.session_id) && step_done(sqlite_, db_, statement, error);
        }
        return true;
    }

    bool insert_audit(AnalyticsEvent const& event, std::string& error) {
        if (!is_audit(event.kind)) {
            return true;
        }
        Statement statement;
        if (!prepare(
                sqlite_,
                db_,
                "INSERT INTO audit_events(event_id,action,cause,before_type,after_type) VALUES(?,?,?,?,?);",
                statement,
                error)) {
            return false;
        }
        return bind_int64(sqlite_, statement.value, 1, static_cast<std::int64_t>(event.sequence))
            && bind_text(sqlite_, statement.value, 2, std::string(to_string(event.kind)))
            && bind_text(sqlite_, statement.value, 3, event.cause)
            && bind_text(sqlite_, statement.value, 4, event.secondary_type)
            && bind_text(sqlite_, statement.value, 5, event.primary_type) && step_done(sqlite_, db_, statement, error);
    }

    sqlite_api::Library sqlite_;
    sqlite_api::sqlite3* db_ = nullptr;
};

StorageWriter::StorageWriter(StorageConfig config)
: config_(std::move(config)),
  queue_(config_.queue_capacity) {}

StorageWriter::~StorageWriter() {
    stop();
}

bool StorageWriter::start() {
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    {
        std::lock_guard lock(state_mutex_);
        if (started_) {
            return ready_ && !stopped_ && !failed_.load(std::memory_order_acquire);
        }
        started_ = true;
    }

    try {
        worker_ = std::thread(&StorageWriter::run, this);
    } catch (...) {
        set_error("could not start SQLite worker");
        return false;
    }
    std::unique_lock lock(state_mutex_);
    state_changed_.wait(lock, [this] { return ready_ || failed_.load(std::memory_order_acquire); });
    return ready_ && !failed_.load(std::memory_order_acquire);
}

void StorageWriter::stop() {
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    {
        std::lock_guard lock(state_mutex_);
        if (!started_) {
            return;
        }
        ready_ = false;
    }
    queue_.close();
    {
        std::lock_guard lock(control_mutex_);
        control_closed_ = true;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard lock(state_mutex_);
    stopped_ = true;
}

bool StorageWriter::submit(AnalyticsEvent event) {
    std::lock_guard lock(state_mutex_);
    if (!ready_ || stopped_ || failed_.load(std::memory_order_acquire)) {
        dropped_events_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    auto result = queue_.try_push(std::move(event));
    if (!result.accepted) {
        dropped_events_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    accepted_events_.fetch_add(1, std::memory_order_relaxed);

    auto previous = last_submitted_sequence_.load(std::memory_order_relaxed);
    while (previous < result.sequence
           && !last_submitted_sequence_.compare_exchange_weak(
               previous, result.sequence, std::memory_order_release, std::memory_order_relaxed)) {
    }
    return true;
}

bool StorageWriter::submit_rollback_transaction(RollbackTransaction transaction) {
    {
        std::scoped_lock lock(state_mutex_, control_mutex_);
        if (!ready_ || control_closed_ || failed_ || control_queue_.size() >= config_.queue_capacity) {
            return false;
        }
        control_queue_.push_back(std::move(transaction));
        pending_control_operations_.fetch_add(1, std::memory_order_release);
    }
    accepted_rollback_transactions_.fetch_add(1, std::memory_order_relaxed);
    state_changed_.notify_all();
    return true;
}

bool StorageWriter::submit_api_token(ApiTokenMetadata token) {
    {
        std::scoped_lock lock(state_mutex_, control_mutex_);
        if (!ready_ || control_closed_ || failed_ || control_queue_.size() >= config_.queue_capacity) {
            return false;
        }
        control_queue_.push_back(std::move(token));
        pending_control_operations_.fetch_add(1, std::memory_order_release);
    }
    accepted_api_tokens_.fetch_add(1, std::memory_order_relaxed);
    state_changed_.notify_all();
    return true;
}

bool StorageWriter::submit_admin_action(AdminActionLogEntry entry) {
    {
        std::scoped_lock lock(state_mutex_, control_mutex_);
        if (!ready_ || control_closed_ || failed_ || control_queue_.size() >= config_.queue_capacity) {
            return false;
        }
        control_queue_.push_back(std::move(entry));
        pending_control_operations_.fetch_add(1, std::memory_order_release);
    }
    accepted_admin_actions_.fetch_add(1, std::memory_order_relaxed);
    state_changed_.notify_all();
    return true;
}

bool StorageWriter::flush() {
    std::unique_lock lock(state_mutex_);
    if (!started_) {
        return false;
    }
    state_changed_.wait(lock, [this] {
        return failed_.load(std::memory_order_acquire)
            || (last_committed_sequence_.load(std::memory_order_acquire)
                    >= last_submitted_sequence_.load(std::memory_order_acquire)
                && pending_control_operations_.load(std::memory_order_acquire) == 0);
    });
    auto events_flushed = last_committed_sequence_.load(std::memory_order_acquire)
        >= last_submitted_sequence_.load(std::memory_order_acquire);
    auto control_flushed = pending_control_operations_.load(std::memory_order_acquire) == 0;
    return !failed_.load(std::memory_order_acquire) && events_flushed && control_flushed;
}

StorageMetrics StorageWriter::metrics() const {
    return {
        accepted_events_.load(std::memory_order_acquire),
        committed_events_.load(std::memory_order_acquire),
        dropped_events_.load(std::memory_order_acquire),
        accepted_rollback_transactions_.load(std::memory_order_acquire),
        committed_rollback_transactions_.load(std::memory_order_acquire),
        accepted_api_tokens_.load(std::memory_order_acquire),
        committed_api_tokens_.load(std::memory_order_acquire),
        accepted_admin_actions_.load(std::memory_order_acquire),
        committed_admin_actions_.load(std::memory_order_acquire),
        failed_.load(std::memory_order_acquire),
    };
}

std::string StorageWriter::last_error() const {
    std::lock_guard lock(state_mutex_);
    return last_error_;
}

void StorageWriter::set_error(std::string message) {
    {
        std::lock_guard lock(state_mutex_);
        last_error_ = std::move(message);
        failed_.store(true, std::memory_order_release);
    }
    state_changed_.notify_all();
}

void StorageWriter::run() {
    try {
        run_worker();
    } catch (std::exception const& exception) {
        set_error(exception.what());
    } catch (...) {
        set_error("unhandled SQLite worker exception");
    }
    {
        std::lock_guard lock(state_mutex_);
        ready_ = false;
        stopped_ = true;
    }
    state_changed_.notify_all();
}

void StorageWriter::run_worker() {
    auto database = std::make_unique<Database>();
    std::string error;
    if (!database->open(config_.database_path, error)) {
        set_error(std::move(error));
        std::lock_guard lock(state_mutex_);
        stopped_ = true;
        state_changed_.notify_all();
        return;
    }

    std::uint64_t sequence = 0;
    if (!database->last_sequence(sequence, error) || !queue_.resume_after(sequence)) {
        set_error(error.empty() ? "could not resume event sequence" : std::move(error));
        return;
    }

    {
        std::lock_guard lock(state_mutex_);
        ready_ = true;
    }
    state_changed_.notify_all();

    auto process_event = [&](EventQueue::EventPtr const& event) {
        error.clear();
        if (!database->insert(*event, error)) {
            set_error(std::move(error));
            return false;
        }
        {
            std::lock_guard lock(state_mutex_);
            committed_events_.fetch_add(1, std::memory_order_relaxed);
            last_committed_sequence_.store(event->sequence, std::memory_order_release);
        }
        state_changed_.notify_all();
        return true;
    };

    auto process_control = [&](ControlOperation const& operation) {
        error.clear();
        auto committed = std::visit(
            [&](auto const& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, RollbackTransaction>) {
                    return database->insert_rollback_transaction(value, error);
                } else if constexpr (std::is_same_v<Value, ApiTokenMetadata>) {
                    return database->insert_api_token(value, error);
                } else {
                    return database->insert_admin_action(value, error);
                }
            },
            operation);
        if (!committed) {
            pending_control_operations_.fetch_sub(1, std::memory_order_release);
            set_error(std::move(error));
            return false;
        }
        std::visit(
            [&](auto const& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, RollbackTransaction>) {
                    committed_rollback_transactions_.fetch_add(1, std::memory_order_relaxed);
                } else if constexpr (std::is_same_v<Value, ApiTokenMetadata>) {
                    committed_api_tokens_.fetch_add(1, std::memory_order_relaxed);
                } else {
                    committed_admin_actions_.fetch_add(1, std::memory_order_relaxed);
                }
            },
            operation);
        {
            std::lock_guard lock(state_mutex_);
            pending_control_operations_.fetch_sub(1, std::memory_order_release);
        }
        state_changed_.notify_all();
        return true;
    };

    for (;;) {
        ControlOperation control;
        bool has_control = false;
        {
            std::lock_guard lock(control_mutex_);
            if (!control_queue_.empty()) {
                control = std::move(control_queue_.front());
                control_queue_.pop_front();
                has_control = true;
            }
        }
        if (has_control) {
            if (!process_control(control)) {
                break;
            }
            continue;
        }

        EventQueue::EventPtr event;
        if (queue_.try_pop(event)) {
            if (!process_event(event)) {
                break;
            }
            continue;
        }

        bool control_done = false;
        {
            std::lock_guard lock(control_mutex_);
            control_done = control_closed_ && control_queue_.empty();
        }
        if (queue_.closed() && queue_.size() == 0 && control_done) {
            break;
        }

        if (queue_.wait_pop_for(event, std::chrono::milliseconds(10)) && event && !process_event(event)) {
            break;
        }
    }

    {
        std::lock_guard lock(state_mutex_);
        stopped_ = true;
    }
    state_changed_.notify_all();
}

} // namespace bedrock_insight
