// Exercises the four exported companion C functions in-process, exactly as a
// native mod would after GetProcAddress. Throwaway DB only.
#include "bedrock_insight/core/remote_api.hpp"
#include "killed_by_fixture.hpp"

#include <sqlite3.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void exec(sqlite3* db, char const* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error == nullptr ? "sqlite error" : error;
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}
bool contains(std::string const& text, std::string const& needle) {
    return text.find(needle) != std::string::npos;
}
} // namespace

int main() {
    auto path = std::filesystem::temp_directory_path() / "bi_remote_api_tests.sqlite3";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    sqlite3* db = nullptr;
    assert(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
    exec(db,
        "CREATE TABLE players(player_id TEXT PRIMARY KEY,last_name TEXT,first_seen_ms INTEGER,last_seen_ms INTEGER);"
        "CREATE TABLE events(event_id INTEGER PRIMARY KEY,occurred_at_ms INTEGER,event_kind TEXT,actor_kind TEXT,"
        "actor_id TEXT,cause TEXT,amount INTEGER,primary_type TEXT,secondary_type TEXT,details_json TEXT);"
        "CREATE TABLE sessions(session_id TEXT PRIMARY KEY,player_id TEXT,started_at_ms INTEGER,ended_at_ms INTEGER,"
        "duration_ms INTEGER);"
        "INSERT INTO players VALUES('uuid64:1:2','Alice',0,1);"
        "INSERT INTO players VALUES('uuid64:3:4','Bob',0,1);"
        "INSERT INTO events VALUES(1,1,'block_mined_result','player','uuid64:1:2',"
        "'ll26.60.gamemode_destroy_block.v1',3,'minecraft:stone','','{}');"
        "INSERT INTO events VALUES(2,1,'block_mined_result','player','uuid64:3:4',"
        "'ll26.60.gamemode_destroy_block.v1',1,'minecraft:dirt','','{}');"
        "INSERT INTO events VALUES(3,1,'player_travel_distance','player','uuid64:1:2',"
        "'ll26.60.poll_position_delta.v1',500,'walk','','{}');");
    bedrock_insight::set_remote_api_database(path);

    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));
    int written = getPlayerStats("Alice", "minecraft:mined", buffer, sizeof(buffer));
    assert(written > 0);
    std::string player{buffer};
    assert(contains(player, "\"ok\":true"));
    assert(contains(player, "\"category\":\"minecraft:mined\""));
    assert(contains(player, "\"player_name\":\"Alice\""));
    assert(contains(player, "\"key\":\"minecraft:stone\""));
    assert(contains(player, "\"value\":3"));
    assert(!contains(player, "uuid64"));
    assert(getPlayerStats("Alice","custom",buffer,sizeof(buffer))>0);
    assert(contains(std::string{buffer},"\"total\":null"));
    assert(contains(std::string{buffer},"closed_session_lower_bound_50ms_ticks"));
    assert(contains(std::string{buffer},"\"player_name\":\"Alice\""));

    std::memset(buffer, 0, sizeof(buffer));
    written = getRankStats("mined", 0, 10, buffer, sizeof(buffer));
    assert(written > 0);
    std::string rank{buffer};
    assert(contains(rank, "\"rank\":1"));
    assert(contains(rank, "\"name\":\"Alice\""));
    assert(contains(rank, "\"name\":\"Bob\""));

    std::memset(buffer, 0, sizeof(buffer));
    assert(getPlayerStatsPage("Bob", "crafted", 0, 10, buffer, sizeof(buffer)) > 0);
    assert(contains(std::string{buffer}, "\"player_name\":\"Bob\""));
    std::memset(buffer, 0, sizeof(buffer));
    assert(getRankStatsPage("mined", 0, 10, buffer, sizeof(buffer)) > 0);
    assert(contains(std::string{buffer}, "\"ok\":true"));
    // CUSTOM ranking requires an explicit key; a bare "custom" is refused.
    std::memset(buffer, 0, sizeof(buffer));
    assert(getRankStatsPage("custom", 0, 10, buffer, sizeof(buffer)) > 0);
    assert(contains(std::string{buffer}, "\"ok\":false"));
    assert(contains(std::string{buffer}, "explicit stat key"));
    std::memset(buffer, 0, sizeof(buffer));
    assert(getRankStatsPage("custom:minecraft:walk_one_cm", 0, 10, buffer, sizeof(buffer)) > 0);
    assert(contains(std::string{buffer}, "\"ok\":true"));
    assert(contains(std::string{buffer}, "\"category\":\"custom:minecraft:walk_one_cm\""));
    assert(contains(std::string{buffer}, "\"value\":500"));

    // Rejections: unknown category, bad paging, missing player, small buffer.
    insert_killer(db,"uuid64:1:2","minecraft:zombie");
    insert_killer(db,"uuid64:1:2","minecraft:skeleton",kKillerChild,"child_projectile");
    assert(getPlayerStats("Alice","killed_by",buffer,sizeof(buffer))>0);
    assert(contains(std::string{buffer},"\"total\":2") && contains(std::string{buffer},"minecraft:zombie"));
    assert(!contains(std::string{buffer},"source_id") && !contains(std::string{buffer},"uuid64"));
    assert(getPlayerStatsPage("Alice","killed_by",1,1,buffer,sizeof(buffer))>0);
    assert(contains(std::string{buffer},"minecraft:zombie") && !contains(std::string{buffer},"minecraft:skeleton"));
    assert(getRankStatsPage("killed_by",0,1,buffer,sizeof(buffer))>0);
    assert(contains(std::string{buffer},"\"value\":2"));
    exec(db,"WITH RECURSIVE n(i) AS (VALUES(0) UNION ALL SELECT i+1 FROM n WHERE i<252) "
        "INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type) "
        "SELECT 1000,'item_pickup_delta','player','uuid64:1:2','ll26.60.take_inventory_delta.v1',1,"
        "printf('minecraft:fixture_%03d',i) FROM n");
    assert(getPlayerStatsPage("Alice","picked_up",20,10,buffer,sizeof(buffer))>0);
    assert(contains(std::string{buffer},"\"total_entries\":253") && contains(std::string{buffer},"\"total\":253"));
    assert(contains(std::string{buffer},"minecraft:fixture_200") && contains(std::string{buffer},"\"has_more\":true"));
    assert(!contains(std::string{buffer},"minecraft:fixture_199") && !contains(std::string{buffer},"minecraft:fixture_210"));
    assert(getPlayerStatsPage("Alice","picked_up",25,10,buffer,sizeof(buffer))>0);
    assert(contains(std::string{buffer},"minecraft:fixture_252") && contains(std::string{buffer},"\"has_more\":false"));
    assert(getPlayerStats("Bob","picked_up",buffer,sizeof(buffer))>0);
    assert(contains(std::string{buffer},"\"total\":null"));
    assert(getPlayerStats("Alice", "not-a-category", buffer, sizeof(buffer)) == -4);
    assert(getRankStats("mined", -1, 10, buffer, sizeof(buffer)) == -2);
    assert(getPlayerStats("Nobody", "mined", buffer, sizeof(buffer)) > 0);
    assert(contains(std::string{buffer}, "\"ok\":false"));
    assert(getRankStats("mined", 0, 10, buffer, 8) == -3);
    assert(buffer[0]=='\0'); // stale success JSON cleared on negative returns
    assert(getPlayerStats(nullptr,"mined",buffer,sizeof(buffer))==-1 && buffer[0]=='\0');

    sqlite3_close(db);
    std::filesystem::remove(path, ignored);
    std::cout << "Remote API PASS: 4 exported functions, JSON units, paging, rejection paths\n";
}
