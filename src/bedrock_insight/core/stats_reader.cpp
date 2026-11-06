#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/closed_session_time.hpp"
#include "bedrock_insight/core/interaction_observation.hpp"
#include "bedrock_insight/core/killed_by_sql.hpp"

#include <chrono>
#include <algorithm>
#include <limits>
#include <utility>

namespace bedrock_insight {
namespace {

// Cooperative SQLite VM deadline, not a claim that OS/disk I/O is preemptible.
// All queries are serialized by this reader's mutex; no persistent handler or
// pointer to a destroyed budget remains on the connection.
struct QueryBudget final {
    sqlite_api::sqlite3* db;
    std::chrono::steady_clock::time_point until=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
    explicit QueryBudget(sqlite_api::sqlite3* value):db(value) {
        sqlite3_progress_handler(db,1000,[](void* context) noexcept {
            return static_cast<int>(std::chrono::steady_clock::now()>=static_cast<QueryBudget*>(context)->until);
        },this);
    }
    ~QueryBudget(){sqlite3_progress_handler(db,0,nullptr,nullptr);}
    QueryBudget(QueryBudget const&)=delete;
    QueryBudget& operator=(QueryBudget const&)=delete;
};

struct Statement final {
    sqlite_api::Library* library = nullptr;
    sqlite_api::sqlite3_stmt* value = nullptr;
    ~Statement() {
        if (value != nullptr && library != nullptr) library->finalize(value);
    }
    Statement() = default;
    Statement(Statement const&) = delete;
    Statement& operator=(Statement const&) = delete;
};

bool prepare(sqlite_api::Library& library, sqlite_api::sqlite3* db, char const* sql,
    Statement& statement, std::string& error) {
    statement.library = &library;
    if (library.prepare_v2(db, sql, -1, &statement.value, nullptr) != sqlite_api::kOk) {
        error = library.errmsg(db);
        return false;
    }
    return true;
}

bool bind_text(sqlite_api::Library& library, sqlite_api::sqlite3_stmt* statement, int index, std::string_view value) {
    return library.bind_text(statement, index, value.data(), static_cast<int>(value.size()),
               reinterpret_cast<sqlite_api::sqlite3_destructor_type>(-1)) == sqlite_api::kOk;
}

// Fixed, comma-separated SQL literal list built ONLY from the compile-time
// interaction-key array. No caller input reaches this text.
std::string const& interaction_key_sql_list() {
    static std::string const value = [] {
        std::string out;
        for (auto key : kInteractionKeys) {
            if (!out.empty()) out += ',';
            out += '\''; out += key; out += '\'';
        }
        return out;
    }();
    return value;
}

// The CUSTOM projection is assembled once from fixed literals so the confirmed
// post-outcome interaction keys and the real-damage producers share one query.
std::string const& custom_type_query() {
    static std::string const sql = std::string(
        "SELECT key, SUM(value) AS value FROM ("
        "SELECT CASE primary_type "
        "WHEN 'walk' THEN 'minecraft:walk_one_cm' WHEN 'sprint' THEN 'minecraft:sprint_one_cm' "
        "WHEN 'crouch' THEN 'minecraft:crouch_one_cm' WHEN 'swim' THEN 'minecraft:swim_one_cm' "
        "WHEN 'fly' THEN 'minecraft:fly_one_cm' WHEN 'aviate' THEN 'minecraft:aviate_one_cm' "
        "WHEN 'fall' THEN 'minecraft:fall_one_cm' WHEN 'climb' THEN 'minecraft:climb_one_cm' "
        "WHEN 'boat' THEN 'minecraft:boat_one_cm' WHEN 'minecart' THEN 'minecraft:minecart_one_cm' "
        "WHEN 'pig' THEN 'minecraft:pig_one_cm' WHEN 'strider' THEN 'minecraft:strider_one_cm' "
        "WHEN 'horse' THEN 'minecraft:horse_one_cm' WHEN 'nautilus' THEN 'minecraft:nautilus_one_cm' "
        "WHEN 'happy_ghast' THEN 'minecraft:happy_ghast_one_cm' "
        "WHEN 'walk_on_water' THEN 'minecraft:walk_on_water_one_cm' "
        "WHEN 'walk_under_water' THEN 'minecraft:walk_under_water_one_cm' ELSE NULL END AS key, amount AS value "
        "FROM events WHERE actor_id=:id AND event_kind='player_travel_distance' "
        "AND cause='ll26.60.poll_position_delta.v1' AND typeof(amount)='integer' AND amount>0 "
        "UNION ALL SELECT 'minecraft:jump', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_jump_impulse' AND cause='ll26.60.jump_pos_delta.v1' "
        "AND typeof(amount)='integer' AND amount=1 "
        "UNION ALL SELECT 'minecraft:mob_kills', 1 FROM events WHERE actor_id=:id "
        "AND event_kind='direct_kill_observation' AND secondary_type='mob' "
        "AND cause IN ('ll26.60.direct_attack_dead_transition.v1','ll26.60.projectile_owner_dead_transition.v1') AND typeof(amount)='integer' AND amount=1 "
        "UNION ALL SELECT 'minecraft:player_kills', 1 FROM events WHERE actor_id=:id "
        "AND event_kind='direct_kill_observation' AND secondary_type='player' "
        "AND cause IN ('ll26.60.direct_attack_dead_transition.v1','ll26.60.projectile_owner_dead_transition.v1') AND typeof(amount)='integer' AND amount=1 "
        "UNION ALL SELECT 'minecraft:deaths', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_death' AND cause='ll26.60.player_dead_transition.v1' "
        "UNION ALL SELECT 'minecraft:leave_game', 1 FROM events WHERE actor_id=:id "
        "AND event_kind='session_end' "
        "UNION ALL SELECT 'minecraft:drop', amount FROM events WHERE actor_id=:id "
        "AND event_kind='item_drop_result' AND cause='ll26.60.player_drop_returned_true.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        "UNION ALL SELECT 'minecraft:sleep_in_bed', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_sleep_result' AND cause='ll26.60.sleep_flag_transition.v1' "
        "AND typeof(amount)='integer' AND amount=1 "
        "UNION ALL SELECT 'minecraft:sneak_time', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_sneak_time' AND cause='ll26.60.sneak_duration.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        // Real health damage in vanilla tenths, attributed by the engine's own
        // damaging-entity id. Attack actions are never substituted here.
        "UNION ALL SELECT 'minecraft:damage_taken', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_damage_taken' AND cause='ll26.60.actor_hurt_event.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        "UNION ALL SELECT 'minecraft:damage_dealt', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_damage_dealt' AND cause='ll26.60.actor_hurt_real_damage.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        // Confirmed block interactions. Every producer is post-origin, so the
        // cause alone already proves the success condition upstream uses; the
        // key list keeps unproduced keys out of the projection.
        "UNION ALL SELECT 'minecraft:'||primary_type, amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_interaction_result' AND cause IN ("
        "'ll26.60.block_use_success.v1','ll26.60.cake_remove_slice.v1',"
        "'ll26.60.cauldron_use_inventory.v1','ll26.60.cauldron_clear_item.v1',"
        "'ll26.60.flower_pot_place.v1','ll26.60.noteblock_attack.v1') "
        "AND primary_type IN (") + interaction_key_sql_list() + ") AND typeof(amount)='integer' AND amount>0 "
        "UNION ALL SELECT 'minecraft:talked_to_villager', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_interaction_result' AND cause='ll26.60.open_trading_villager.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        "UNION ALL SELECT 'minecraft:animals_bred', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_interaction_result' AND cause='ll26.60.breed_offspring.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        "UNION ALL SELECT 'minecraft:target_hit', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_interaction_result' AND cause='ll26.60.target_block_projectile_hit.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        "UNION ALL SELECT 'minecraft:traded_with_villager', amount FROM events WHERE actor_id=:id "
        "AND event_kind='player_interaction_result' AND cause='ll26.60.trade_result_transfer_success.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        // World time: cumulative real Level ticks persisted by the busy-tick
        // sampler. This is a WORLD value shared by every player (as in vanilla),
        // NOT the player's play_time. It only counts ticks observed by this
        // capture and never extends an offline/open session to wall clock.
        "UNION ALL SELECT 'minecraft:total_world_time', SUM(amount) FROM events "
        "WHERE event_kind='server_tick' AND cause='ll26.60.busytick.v1' "
        "AND typeof(amount)='integer' AND amount>0 "
        ") WHERE key IS NOT NULL AND value IS NOT NULL GROUP BY key "
        "ORDER BY value DESC, key COLLATE BINARY";
    return sql;
}

// Fixed SQL per category. Only literal event kinds/causes are embedded; the
// player identifier and pagination are always bound parameters.
char const* type_query(StatsCategory category) {
    switch (category) {
    case StatsCategory::Mined:
        return "SELECT primary_type AS key, SUM(amount) AS value FROM events "
               "WHERE actor_id=:id AND event_kind='block_mined_result' "
               "AND cause='ll26.60.gamemode_destroy_block.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY primary_type ORDER BY value DESC, key COLLATE BINARY";
    case StatsCategory::Crafted:
        return "SELECT primary_type AS key, SUM(amount) AS value FROM events "
               "WHERE actor_id=:id AND event_kind='item_craft_result' "
               "AND cause='ll26.60.craft_request_success.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY primary_type ORDER BY value DESC, key COLLATE BINARY";
    case StatsCategory::Used:
        return "SELECT primary_type AS key, SUM(amount) AS value FROM events "
               "WHERE actor_id=:id AND event_kind='item_use_result' "
               "AND cause='ll26.60.gamemode_use_return.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY primary_type ORDER BY value DESC, key COLLATE BINARY";
    case StatsCategory::PickedUp:
        return "SELECT primary_type AS key, SUM(amount) AS value FROM events "
               "WHERE actor_id=:id AND event_kind='item_pickup_delta' "
               "AND cause='ll26.60.take_inventory_delta.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY primary_type ORDER BY value DESC, key COLLATE BINARY";
    case StatsCategory::Dropped:
        return "SELECT primary_type AS key, SUM(amount) AS value FROM events "
               "WHERE actor_id=:id AND event_kind='item_drop_result' "
               "AND cause='ll26.60.player_drop_returned_true.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY primary_type ORDER BY value DESC, key COLLATE BINARY";
    case StatsCategory::Killed:
        return "SELECT primary_type AS key, COUNT(*) AS value FROM events "
               "WHERE actor_id=:id AND event_kind='direct_kill_observation' "
               "AND cause IN ('ll26.60.direct_attack_dead_transition.v1','ll26.60.projectile_owner_dead_transition.v1') AND typeof(amount)='integer' AND amount=1 "
               "GROUP BY primary_type ORDER BY value DESC, key COLLATE BINARY";
    case StatsCategory::KilledBy:
        {
            static std::string const sql=std::string("SELECT primary_type AS key,COUNT(*) AS value FROM events ")+
                "WHERE actor_id=:id AND "+kKilledByPredicate+
                " GROUP BY primary_type ORDER BY value DESC,key COLLATE BINARY";
            return sql.c_str();
        }
    case StatsCategory::Broken:
        return "SELECT primary_type AS key, COUNT(*) AS value FROM events "
               "WHERE actor_id=:id AND event_kind='item_broken_result' "
               "AND cause='ll26.60.durability_item_destroyed.v1' AND typeof(amount)='integer' AND amount=1 "
               "GROUP BY primary_type ORDER BY value DESC, key COLLATE BINARY";
    case StatsCategory::Custom:
        return custom_type_query().c_str();
    default:
        return nullptr;
    }
}

char const* ranking_query(StatsCategory category) {
    switch (category) {
    case StatsCategory::Mined:
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='block_mined_result' "
               "AND cause='ll26.60.gamemode_destroy_block.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    case StatsCategory::Crafted:
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='item_craft_result' "
               "AND cause='ll26.60.craft_request_success.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    case StatsCategory::Used:
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='item_use_result' "
               "AND cause='ll26.60.gamemode_use_return.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    case StatsCategory::PickedUp:
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='item_pickup_delta' "
               "AND cause='ll26.60.take_inventory_delta.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    case StatsCategory::Dropped:
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='item_drop_result' "
               "AND cause='ll26.60.player_drop_returned_true.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    case StatsCategory::Killed:
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='direct_kill_observation' "
               "AND cause IN ('ll26.60.direct_attack_dead_transition.v1','ll26.60.projectile_owner_dead_transition.v1') AND typeof(amount)='integer' AND amount=1 "
               "GROUP BY actor_id";
    case StatsCategory::KilledBy:
        {
            static std::string const sql=std::string("SELECT actor_id,COUNT(*) AS value FROM events WHERE ")+
                kKilledByPredicate+" GROUP BY actor_id";
            return sql.c_str();
        }
    case StatsCategory::Broken:
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='item_broken_result' "
               "AND cause='ll26.60.durability_item_destroyed.v1' AND typeof(amount)='integer' AND amount=1 "
               "GROUP BY actor_id";
    default:
        return nullptr;
    }
}

// Custom rankings are only meaningful for ONE explicit key: adding centimeters
// and damage tenths would silently compare incompatible units.
std::string custom_ranking_query(std::string_view key, std::string& error) {
    struct TravelKey final { std::string_view key; std::string_view mode; };
    static constexpr TravelKey kTravel[] = {
        {"minecraft:walk_one_cm", "walk"}, {"minecraft:sprint_one_cm", "sprint"},
        {"minecraft:crouch_one_cm", "crouch"}, {"minecraft:swim_one_cm", "swim"},
        {"minecraft:fly_one_cm", "fly"}, {"minecraft:aviate_one_cm", "aviate"},
        {"minecraft:fall_one_cm", "fall"}, {"minecraft:climb_one_cm", "climb"},
        {"minecraft:boat_one_cm", "boat"}, {"minecraft:minecart_one_cm", "minecart"},
        {"minecraft:pig_one_cm", "pig"}, {"minecraft:strider_one_cm", "strider"},
        {"minecraft:horse_one_cm", "horse"}, {"minecraft:nautilus_one_cm", "nautilus"},
        {"minecraft:happy_ghast_one_cm", "happy_ghast"},
        {"minecraft:walk_on_water_one_cm", "walk_on_water"},
        {"minecraft:walk_under_water_one_cm", "walk_under_water"},
    };
    for (auto const& entry : kTravel) {
        if (entry.key == key) {
            return "SELECT actor_id, SUM(amount) AS value FROM events "
                   "WHERE event_kind='player_travel_distance' AND cause='ll26.60.poll_position_delta.v1' "
                   "AND primary_type='" + std::string(entry.mode) + "' "
                   "AND typeof(amount)='integer' AND amount>0 GROUP BY actor_id";
        }
    }
    // Confirmed interactions only. Each key is a compile-time literal from
    // kInteractionKeys and carries exactly the post-origin cause that upstream
    // uses for it, so an unrelated interaction row can never satisfy it.
    for (auto const& interaction : kInteractionKeys) {
        if (key == "minecraft:" + std::string(interaction)) {
            return "SELECT actor_id, SUM(amount) AS value FROM events "
                   "WHERE event_kind='player_interaction_result' "
                   "AND cause='" + std::string(interaction_key_cause(interaction)) + "' "
                   "AND primary_type='" + std::string(interaction) + "' "
                   "AND typeof(amount)='integer' AND amount>0 GROUP BY actor_id";
        }
    }
    if (key == "minecraft:damage_taken") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='player_damage_taken' "
               "AND cause='ll26.60.actor_hurt_event.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    }
    if (key == "minecraft:damage_dealt") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='player_damage_dealt' "
               "AND cause='ll26.60.actor_hurt_real_damage.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    }
    if (key == "minecraft:drop") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='item_drop_result' "
               "AND cause='ll26.60.player_drop_returned_true.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    }
    if (key == "minecraft:jump") {
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='player_jump_impulse' "
               "AND cause='ll26.60.jump_pos_delta.v1' AND typeof(amount)='integer' AND amount=1 GROUP BY actor_id";
    }
    if (key == "minecraft:mob_kills") {
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='direct_kill_observation' "
               "AND secondary_type='mob' AND cause IN ('ll26.60.direct_attack_dead_transition.v1','ll26.60.projectile_owner_dead_transition.v1') "
               "AND typeof(amount)='integer' AND amount=1 GROUP BY actor_id";
    }
    if (key == "minecraft:player_kills") {
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='direct_kill_observation' "
               "AND secondary_type='player' AND cause IN ('ll26.60.direct_attack_dead_transition.v1','ll26.60.projectile_owner_dead_transition.v1') "
               "AND typeof(amount)='integer' AND amount=1 GROUP BY actor_id";
    }
    if (key == "minecraft:deaths") {
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='player_death' "
               "AND cause='ll26.60.player_dead_transition.v1' GROUP BY actor_id";
    }
    if (key == "minecraft:leave_game") {
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='session_end' GROUP BY actor_id";
    }
    if (key == "minecraft:sleep_in_bed") {
        return "SELECT actor_id, COUNT(*) AS value FROM events WHERE event_kind='player_sleep_result' "
               "AND cause='ll26.60.sleep_flag_transition.v1' AND typeof(amount)='integer' AND amount=1 "
               "GROUP BY actor_id";
    }
    if (key == "minecraft:sneak_time") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='player_sneak_time' "
               "AND cause='ll26.60.sneak_duration.v1' AND typeof(amount)='integer' AND amount>0 GROUP BY actor_id";
    }
    if (key == "minecraft:talked_to_villager") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='player_interaction_result' "
               "AND cause='ll26.60.open_trading_villager.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    }
    if (key == "minecraft:animals_bred") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='player_interaction_result' "
               "AND cause='ll26.60.breed_offspring.v1' AND typeof(amount)='integer' AND amount>0 "
               "GROUP BY actor_id";
    }
    if (key == "minecraft:target_hit") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='player_interaction_result' "
               "AND cause='ll26.60.target_block_projectile_hit.v1' "
               "AND typeof(amount)='integer' AND amount>0 GROUP BY actor_id";
    }
    if (key == "minecraft:traded_with_villager") {
        return "SELECT actor_id, SUM(amount) AS value FROM events WHERE event_kind='player_interaction_result' "
               "AND cause='ll26.60.trade_result_transfer_success.v1' "
               "AND typeof(amount)='integer' AND amount>0 GROUP BY actor_id";
    }
    if (key == "minecraft:total_world_time") {
        // A shared world tick count cannot be ranked per player; ranking it
        // would fabricate an identical score for everyone.
        error = "minecraft:total_world_time is a world value and is not per-player rankable";
        return {};
    }
    if (key.empty()) {
        error = "custom ranking requires an explicit stat key";
    } else {
        error = "custom key is not rankable or unknown";
    }
    return {};
}

} // namespace

StatsReader::StatsReader(std::filesystem::path database_path)
: database_path_(std::move(database_path)) {}

StatsReader::~StatsReader() { close(); }

bool StatsReader::open(std::string& error) {
    std::lock_guard lock(mutex_);
    if (db_ != nullptr) return true;
    if (!std::filesystem::exists(database_path_)) {
        error = "database does not exist";
        return false;
    }
    auto result = sqlite_.open_v2(database_path_.string().c_str(), &db_,
        sqlite_api::kReadOnly | sqlite_api::kFullMutex, nullptr);
    if (result != sqlite_api::kOk) {
        error = db_ == nullptr ? "sqlite stats open failed" : sqlite_.errmsg(db_);
        return false;
    }
    sqlite_.busy_timeout(db_, 50);
    return true;
}

void StatsReader::close() {
    std::lock_guard lock(mutex_);
    if (db_ != nullptr) { sqlite_.close(db_); db_ = nullptr; }
}

bool StatsReader::find_player(std::string_view query, std::string& player_id,
    std::string& display_name, std::string& error) const {
    std::lock_guard lock(mutex_);
    if (db_ == nullptr) { error = "stats reader is not open"; return false; }
    QueryBudget budget(db_);
    Statement statement;
    if (!prepare(sqlite_, db_,
            "SELECT player_id,last_name FROM players WHERE player_id=? OR last_name=? "
            "ORDER BY last_seen_ms DESC LIMIT 1", statement, error)) {
        return false;
    }
    if (!bind_text(sqlite_, statement.value, 1, query) || !bind_text(sqlite_, statement.value, 2, query)
        || sqlite_.step(statement.value) != sqlite_api::kRow) {
        error = "player not found";
        return false;
    }
    player_id = sqlite_.column_text(statement.value, 0) == nullptr ? "" : sqlite_.column_text(statement.value, 0);
    display_name = sqlite_.column_text(statement.value, 1) == nullptr ? "" : sqlite_.column_text(statement.value, 1);
    if (player_id.empty()) { error = "player not found"; return false; }
    return true;
}

bool StatsReader::player_summary(std::string_view player_id, StatsCategory category,
    PlayerStatSummary& result, std::string& error, std::size_t offset,std::size_t limit) const {
    // The input may alias result.player_id (/stats does this).
    std::string const owned_id(player_id);
    result = {};
    result.category = category;
    result.offset=offset;result.limit=limit;
    result.total_available = category != StatsCategory::Custom;
    error.clear();
    if(limit<1 || limit>200 || offset>100000){error="player page outside bounds";return false;}
    auto* sql = type_query(category);
    if (sql == nullptr) { error = "unsupported category"; return false; }
    std::lock_guard lock(mutex_);
    if (db_ == nullptr) { error = "stats reader is not open"; return false; }
    QueryBudget budget(db_);
    Statement name;
    if (!prepare(sqlite_,db_,"SELECT last_name FROM players WHERE player_id=?",name,error) ||
        !bind_text(sqlite_,name.value,1,owned_id) || sqlite_.step(name.value)!=sqlite_api::kRow) {
        if(error.empty())error="player not found";
        return false;
    }
    auto const* stored_name=sqlite_.column_text(name.value,0);
    result.display_name=stored_name==nullptr?"":stored_name;
    Statement statement;
    if (!prepare(sqlite_, db_, sql, statement, error)
        || !bind_text(sqlite_, statement.value, 1, owned_id)) {
        return false;
    }
    result.player_id = owned_id;
    int step;
    while ((step=sqlite_.step(statement.value)) == sqlite_api::kRow) {
        StatRow row;
        auto* key = sqlite_.column_text(statement.value, 0);
        row.key = key == nullptr ? "" : key;
        row.value = sqlite_.column_int64(statement.value, 1);
        if (row.key.empty() || row.value <= 0) continue;
        if(++result.row_count>100000){error="type cardinality exceeds reader budget";result.rows.clear();return false;}
        if(result.total_available)result.total = row.value > std::numeric_limits<std::int64_t>::max()-result.total
            ? std::numeric_limits<std::int64_t>::max() : result.total+row.value;
        if(category==StatsCategory::Custom || (result.row_count>offset && result.rows.size()<limit))
            result.rows.push_back(std::move(row));
    }
    if(step!=sqlite_api::kDone){error=sqlite_.errmsg(db_);result.rows.clear();return false;}
    if (category == StatsCategory::Custom) {
        // Closed-session lower bounds, never abandoned open sessions or offline
        // wall time. Both counters and their rankings share identical SQL.
        auto const now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        Statement sessions;
        auto sql_time=closed_session_times_sql(now_ms)+
            "SELECT play_ms/50,since_death_ms/50 FROM times WHERE player_id=?";
        if(!prepare(sqlite_,db_,sql_time.c_str(),sessions,error) ||
            !bind_text(sqlite_,sessions.value,1,owned_id))return false;
        step=sqlite_.step(sessions.value);
        if(step==sqlite_api::kRow) {
            auto const play=sqlite_.column_int64(sessions.value,0);
            auto const since=sqlite_.column_int64(sessions.value,1);
            if(play>0)result.rows.push_back({"minecraft:play_time",play});
            if(since>0)result.rows.push_back({"minecraft:time_since_death",since});
        } else if(step!=sqlite_api::kDone){error=sqlite_.errmsg(db_);return false;}
    }
    if(category==StatsCategory::Custom) {
        std::sort(result.rows.begin(),result.rows.end(),[](auto const& a,auto const& b){
            return a.value!=b.value?a.value>b.value:a.key<b.key;
        });
        result.row_count=result.rows.size();
        auto const begin=std::min(offset,result.rows.size());
        auto const end=std::min(result.rows.size(),begin+limit);
        std::vector<StatRow> page;
        for(auto i=begin;i<end;++i)page.push_back(std::move(result.rows[i]));
        result.rows=std::move(page);
    }
    result.total_available=result.total_available && result.row_count>0;
    return true;
}

bool StatsReader::ranking(StatsCategory category, std::string_view custom_key, std::size_t offset,
    std::size_t limit, std::vector<RankingEntry>& result, std::string& error) const {
    result.clear();
    error.clear();
    if(limit<1 || limit>100 || offset>100000){error="ranking page outside bounds";return false;}
    std::string source;
    if (category == StatsCategory::Custom) {
        if(custom_key=="minecraft:play_time" || custom_key=="minecraft:time_since_death") {
            auto const now=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            source=closed_session_times_sql(now)+"SELECT player_id AS actor_id,"+
                (custom_key=="minecraft:play_time"?"play_ms":"since_death_ms")+std::string("/50 AS value FROM times");
        } else source = custom_ranking_query(custom_key, error);
        if (source.empty()) return false;
    } else {
        auto* fixed = ranking_query(category);
        if (fixed == nullptr) { error = "unsupported category"; return false; }
        source = fixed;
    }
    std::lock_guard lock(mutex_);
    if (db_ == nullptr) { error = "stats reader is not open"; return false; }
    QueryBudget budget(db_);
    std::string sql = std::string("SELECT r.value,p.last_name FROM (") + source
        + ") r JOIN players p ON p.player_id=r.actor_id WHERE r.value>0 "
          "ORDER BY r.value DESC,p.last_name COLLATE BINARY,r.actor_id LIMIT ? OFFSET ?";
    Statement statement;
    if (!prepare(sqlite_, db_, sql.c_str(), statement, error)
        || sqlite_.bind_int64(statement.value, 1, static_cast<long long>(limit)) != sqlite_api::kOk
        || sqlite_.bind_int64(statement.value, 2, static_cast<long long>(offset)) != sqlite_api::kOk) {
        if (error.empty()) error = sqlite_.errmsg(db_);
        return false;
    }
    int step;
    while ((step=sqlite_.step(statement.value)) == sqlite_api::kRow) {
        RankingEntry entry;
        entry.value = sqlite_.column_int64(statement.value, 0);
        auto* name = sqlite_.column_text(statement.value, 1);
        entry.display_name = name == nullptr ? "" : name;
        if (entry.value > 0) result.push_back(std::move(entry));
    }
    if(step!=sqlite_api::kDone){error=sqlite_.errmsg(db_);result.clear();return false;}
    return true;
}

} // namespace bedrock_insight
