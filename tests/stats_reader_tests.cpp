// Exercises the read-only StatsReader that backs /stats and the exported API.
// Uses a throwaway SQLite file; never touches a live BDS database.
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/stats_page.hpp"
#include "killed_by_fixture.hpp"

#include <sqlite3.h>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bedrock_insight;

namespace {
void exec(sqlite3* db, char const* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error == nullptr ? "sqlite error" : error;
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}
void event(sqlite3* db, char const* kind, char const* cause, char const* primary,
    char const* secondary, long long amount, char const* player) {
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type,"
        "secondary_type,details_json) VALUES(1000,'%s','player','%s','%s',%lld,'%s','%s','{}');",
        kind, player, cause, amount, primary, secondary);
    exec(db, sql);
}
} // namespace

int main() {
    using namespace bedrock_insight;
    for(auto key:kCustomKeys)assert(custom_key_label(key)!=key); // independent Japanese labels for all77
    assert(custom_key_label("addon:unknown")=="addon:unknown");
    auto empty=stats_page(0,99,12);assert(empty.begin==0 && empty.end==0 && empty.pages==1 && !empty.next);
    auto keys=stats_page(77,6,12);assert(keys.page==6 && keys.begin==72 && keys.end==77 && keys.previous && !keys.next);
    auto first=stats_page(77,0,12);assert(first.next && !first.previous && first.end==12);
    auto clamped=stats_page(77,999,0);assert(clamped.page==76 && clamped.begin==76 && clamped.end==77);
    auto path = std::filesystem::temp_directory_path() / "bi_stats_reader_tests.sqlite3";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    sqlite3* db = nullptr;
    assert(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
    exec(db,
        "CREATE TABLE players(player_id TEXT PRIMARY KEY,last_name TEXT,first_seen_ms INTEGER,last_seen_ms INTEGER);"
        "CREATE TABLE events(event_id INTEGER PRIMARY KEY,occurred_at_ms INTEGER,event_kind TEXT,actor_kind TEXT,"
        "actor_id TEXT,cause TEXT,amount INTEGER,primary_type TEXT,secondary_type TEXT,details_json TEXT);"
        "CREATE TABLE sessions(session_id TEXT PRIMARY KEY,player_id TEXT,started_at_ms INTEGER,ended_at_ms INTEGER,"
        "duration_ms INTEGER);");
    exec(db,
        "INSERT INTO players VALUES('uuid64:1:2','Alice',0,1000);"
        "INSERT INTO players VALUES('uuid64:3:4','Bob',0,1000);");
    event(db, "block_mined_result", "ll26.60.gamemode_destroy_block.v1", "minecraft:stone", "", 1, "uuid64:1:2");
    event(db, "block_mined_result", "ll26.60.gamemode_destroy_block.v1", "minecraft:stone", "", 1, "uuid64:1:2");
    event(db, "block_mined_result", "ll26.60.gamemode_destroy_block.v1", "minecraft:dirt", "", 1, "uuid64:3:4");
    event(db, "item_craft_result", "ll26.60.craft_request_success.v1", "minecraft:stick", "", 3, "uuid64:1:2");
    // The current 26.60 producer uses different post-transfer causes from the
    // historical craft_request_success rows. All are actual crafted outputs.
    event(db, "item_craft_result", "ll26.60.created_output_transfer.v1", "minecraft:planks", "workbench", 4,
        "uuid64:1:2");
    event(db, "item_craft_result", "ll26.60.furnace_output_extract.v1", "minecraft:iron_ingot", "minecraft:furnace", 2,
        "uuid64:1:2");
    event(db, "item_use_result", "ll26.60.player_use_item_consumed.v1", "minecraft:snowball", "throw", 1,
        "uuid64:1:2");
    event(db, "item_use_result", "ll26.60.item_hurt_used.v1", "minecraft:iron_pickaxe", "durability", 1,
        "uuid64:1:2");
    event(db, "item_use_result", "ll26.60.fake_use_attempt.v1", "minecraft:snowball", "", 1, "uuid64:1:2");
    event(db, "player_travel_distance", "ll26.60.poll_position_delta.v1", "walk", "", 500, "uuid64:1:2");
    event(db, "player_travel_distance", "ll26.60.poll_position_delta.v1", "walk_on_water", "", 250, "uuid64:1:2");
    event(db, "player_travel_distance", "ll26.60.poll_position_delta.v1", "walk_under_water", "", 350,
        "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.player_interact_block_event.v1", "open_chest",
        "minecraft:chest", 1, "uuid64:1:2");
    // Confirmed post-outcome interaction (Block::use returned true) IS counted;
    // the pre-outcome right-click notification above never becomes a value.
    event(db, "player_interaction_result", "ll26.60.block_use_success.v1", "open_barrel",
        "minecraft:barrel", 1, "uuid64:1:2");
    // A dedicated hook producer counts only under its own key/cause.
    event(db, "player_interaction_result", "ll26.60.cake_remove_slice.v1", "eat_cake_slice",
        "minecraft:cake", 1, "uuid64:1:2");
    event(db, "player_damage_taken", "ll26.60.actor_hurt_event.v1", "cause:4", "entity", 20, "uuid64:1:2");
    event(db, "player_damage_dealt", "ll26.60.actor_hurt_real_damage.v1", "minecraft:zombie", "cause:2", 30,
        "uuid64:1:2");
    // Attack actions / panic causes must never masquerade as real damage.
    event(db, "player_damage_dealt", "ll26.60.player_attack_event.v1", "minecraft:zombie", "", 1, "uuid64:1:2");
    event(db, "item_drop_result", "ll26.60.player_drop_returned_true.v1", "minecraft:stone", "", 4, "uuid64:1:2");
    event(db, "player_sleep_result", "ll26.60.sleep_flag_transition.v1", "minecraft:sleep_in_bed", "", 1,
        "uuid64:1:2");
    event(db, "player_sneak_time", "ll26.60.sneak_duration.v1", "sneak", "", 120, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.open_trading_villager.v1",
        "minecraft:talked_to_villager", "minecraft:villager", 1, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.breed_offspring.v1",
        "minecraft:animals_bred", "minecraft:cow", 1, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.target_block_projectile_hit.v1",
        "minecraft:target_hit", "minecraft:arrow", 1, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.trade_result_transfer_success.v1",
        "minecraft:traded_with_villager", "trade_result", 1, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.trade_created_output.v1",
        "minecraft:traded_with_villager", "trade_output", 1, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.play_record_place.v1",
        "minecraft:play_record", "minecraft:music_disc_13", 1, "uuid64:1:2");
    // A committed enchantment output is visible in both CUSTOM details and
    // rankings; the wrong cause or container type must never count.
    event(db, "player_interaction_result", "ll26.60.enchant_output_transfer.v1",
        "minecraft:enchant_item", "enchantment", 1, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.enchant_output_transfer.v1",
        "minecraft:enchant_item", "other", 1, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.enchant_output_transfer.v1",
        "minecraft:enchant_item", "enchantment", 2, "uuid64:1:2");
    event(db, "player_interaction_result", "ll26.60.fake_enchant_attempt.v1",
        "minecraft:enchant_item", "enchantment", 1, "uuid64:1:2");
    event(db, "item_broken_result", "ll26.60.durability_item_destroyed.v1", "minecraft:iron_pickaxe", "", 1,
        "uuid64:1:2");
    // Two persisted busy-tick windows sum to the shared world time (real ticks).
    event(db, "server_tick", "ll26.60.busytick.v1", "", "", 600, "uuid64:1:2");
    event(db, "server_tick", "ll26.60.busytick.v1", "", "", 400, "uuid64:1:2");
    // Interval telemetry must NOT be added on top of the busy-tick world time.
    event(db, "server_tick", "ll26.60.interval.v1", "", "", 999, "uuid64:1:2");
    exec(db, "INSERT INTO sessions VALUES('s1','uuid64:1:2',0,10000,10000);");

    StatsReader reader{path};
    std::string error;
    assert(reader.open(error));
    std::string id, name;
    assert(reader.find_player("Alice", id, name, error));
    assert(id == "uuid64:1:2" && name == "Alice");
    assert(reader.find_player("uuid64:3:4", id, name, error) && name == "Bob");
    assert(!reader.find_player("Nobody", id, name, error));

    PlayerStatSummary mined;
    assert(reader.player_summary("uuid64:1:2", StatsCategory::Mined, mined, error));
    assert(mined.display_name=="Alice" && mined.total_available);
    assert(reader.player_summary(mined.player_id,StatsCategory::Mined,mined,error)); // aliased /stats input
    assert(mined.player_id=="uuid64:1:2" && mined.display_name=="Alice");
    assert(mined.total == 2);
    assert(mined.rows.size() == 1 && mined.rows[0].key == "minecraft:stone" && mined.rows[0].value == 2);

    PlayerStatSummary crafted;
    assert(reader.player_summary("uuid64:1:2", StatsCategory::Crafted, crafted, error));
    assert(crafted.total_available && crafted.total == 9 && crafted.rows.size() == 3);
    PlayerStatSummary used;
    assert(reader.player_summary("uuid64:1:2", StatsCategory::Used, used, error));
    assert(used.total_available && used.total == 2 && used.rows.size() == 2);

    PlayerStatSummary custom;
    assert(reader.player_summary("uuid64:1:2", StatsCategory::Custom, custom, error));
    bool has_walk = false, has_play = false, has_drop = false, has_chest = false, has_damage = false;
    bool has_since_death = false, has_sleep = false, has_sneak = false, has_dealt = false;
    bool has_talked = false, has_bred = false, has_target = false, has_barrel = false, has_traded = false;
    bool has_enchant = false, has_record = false;
    bool has_on_water = false, has_under_water = false, has_world_time = false, has_cake = false;
    for (auto const& row : custom.rows) {
        if (row.key == "minecraft:eat_cake_slice") { has_cake = row.value == 1; }
        if (row.key == "minecraft:talked_to_villager") { has_talked = row.value == 1; }
        if (row.key == "minecraft:animals_bred") { has_bred = row.value == 1; }
        if (row.key == "minecraft:target_hit") { has_target = row.value == 1; }
        if (row.key == "minecraft:traded_with_villager") { has_traded = row.value == 2; }
        if (row.key == "minecraft:play_record") { has_record = row.value == 1; }
        if (row.key == "minecraft:enchant_item") { has_enchant = row.value == 1; }
        if (row.key == "minecraft:walk_on_water_one_cm") { has_on_water = row.value == 250; }
        if (row.key == "minecraft:walk_under_water_one_cm") { has_under_water = row.value == 350; }
        if (row.key == "minecraft:total_world_time") { has_world_time = row.value == 1000; }
        if (row.key == "minecraft:walk_one_cm") { has_walk = row.value == 500; }
        if (row.key == "minecraft:play_time") { has_play = row.value == 200; }
        if (row.key == "minecraft:time_since_death") { has_since_death = row.value == 200; }
        if (row.key == "minecraft:drop") { has_drop = row.value == 4; }
        if (row.key == "minecraft:open_chest") { has_chest = true; }
        if (row.key == "minecraft:open_barrel") { has_barrel = row.value == 1; }
        if (row.key == "minecraft:damage_taken") { has_damage = row.value == 20; }
        if (row.key == "minecraft:damage_dealt") { has_dealt = row.value == 30; } // real, not attack actions
        if (row.key == "minecraft:sleep_in_bed") { has_sleep = row.value == 1; }
        if (row.key == "minecraft:sneak_time") { has_sneak = row.value == 120; }
    }
    assert(has_walk && has_play && has_since_death && has_drop && !has_chest && has_barrel && has_damage
        && has_dealt && has_sleep && has_sneak && has_talked && has_bred && has_target && has_traded
        && has_on_water && has_under_water && has_world_time && has_cake && has_enchant && has_record);
    assert(!custom.total_available && custom.total==0); // not cm + counts + damage + ticks
    // World time is a shared world value, never rankable per player.
    std::vector<RankingEntry> world_rank;
    assert(!reader.ranking(StatsCategory::Custom, "minecraft:total_world_time", 0, 10, world_rank, error));
    assert(world_rank.empty());

    PlayerStatSummary broken;
    assert(reader.player_summary("uuid64:1:2", StatsCategory::Broken, broken, error));
    assert(broken.total == 1 && broken.rows.size() == 1
        && broken.rows[0].key == "minecraft:iron_pickaxe");

    std::vector<RankingEntry> ranking;
    assert(reader.ranking(StatsCategory::Crafted, {}, 0, 10, ranking, error));
    assert(ranking.size() == 1 && ranking[0].value == 9);
    assert(reader.ranking(StatsCategory::Used, {}, 0, 10, ranking, error));
    assert(ranking.size() == 1 && ranking[0].value == 2);
    assert(reader.ranking(StatsCategory::Mined, {}, 0, 10, ranking, error));
    assert(ranking.size() == 2);
    assert(ranking[0].display_name == "Alice" && ranking[0].value == 2);
    assert(ranking[1].display_name == "Bob" && ranking[1].value == 1);
    std::vector<RankingEntry> page2;
    assert(reader.ranking(StatsCategory::Mined, {}, 2, 10, page2, error) && page2.empty());

    // Generic CUSTOM ranking is refused; only an explicit key is rankable.
    // Interaction pre-notifications are NOT confirmed successes and stay empty.
    std::vector<RankingEntry> custom_rank;
    assert(!reader.ranking(StatsCategory::Custom, {}, 0, 10, custom_rank, error));
    assert(custom_rank.empty());
    assert(reader.ranking(StatsCategory::Custom, "minecraft:play_time", 0, 10, custom_rank, error));
    assert(custom_rank.size()==1 && custom_rank[0].value==200);
    // The only open_chest row is the pre-outcome notification: rankable key but
    // no confirmed row, so the result is an empty (not fabricated) list.
    assert(reader.ranking(StatsCategory::Custom,"minecraft:open_chest",0,10,custom_rank,error) && custom_rank.empty());
    // Confirmed open_barrel and the real-damage producers are rankable.
    assert(reader.ranking(StatsCategory::Custom,"minecraft:open_barrel",0,10,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].value==1);
    // A dedicated-hook key is only satisfied by its own cause, not by the
    // Block::use producer, so the two producers can never be double counted.
    assert(reader.ranking(StatsCategory::Custom,"minecraft:eat_cake_slice",0,10,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].value==1);
    assert(reader.ranking(StatsCategory::Custom,"minecraft:damage_taken",0,10,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].value==20);
    assert(reader.ranking(StatsCategory::Custom,"minecraft:damage_dealt",0,10,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].value==30 && custom_rank[0].display_name=="Alice");
    assert(reader.ranking(StatsCategory::Custom, "minecraft:walk_one_cm", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].display_name == "Alice" && custom_rank[0].value == 500);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:walk_on_water_one_cm", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].value == 250);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:walk_under_water_one_cm", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].value == 350);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:talked_to_villager", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].value == 1);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:animals_bred", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].value == 1);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:target_hit", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].value == 1);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:traded_with_villager", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].value == 2);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:play_record", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].value == 1);
    assert(reader.ranking(StatsCategory::Custom, "minecraft:enchant_item", 0, 10, custom_rank, error));
    assert(custom_rank.size() == 1 && custom_rank[0].display_name == "Alice" && custom_rank[0].value == 1);

    // Closed intervals merge duplicates/nesting/chains, but not gaps. Abandoned
    // open, negative, malformed and future-ended sessions NEVER grow to now.
    exec(db,"INSERT INTO sessions VALUES('nested','uuid64:1:2',2000,6000,4000);"
        "INSERT INTO sessions VALUES('chain1','uuid64:1:2',9000,12000,3000);"
        "INSERT INTO sessions VALUES('chain2','uuid64:1:2',11500,20000,8500);"
        "INSERT INTO sessions VALUES('dup','uuid64:1:2',0,10000,10000);"
        "INSERT INTO sessions VALUES('gap','uuid64:1:2',30000,40000,10000);"
        "INSERT INTO sessions VALUES('open','uuid64:1:2',0,NULL,NULL);"
        "INSERT INTO sessions VALUES('bad','uuid64:1:2',0,100000,2);"
        "INSERT INTO sessions VALUES('negative','uuid64:1:2',-100,10000,10100);"
        "INSERT INTO sessions VALUES('future','uuid64:1:2',0,9000000000000000,9000000000000000);"
        "INSERT INTO sessions VALUES('bob','uuid64:3:4',0,30000,30000);");
    event(db,"player_death","ll26.60.player_dead_transition.v1","minecraft:player","cause:5",1,"uuid64:1:2");
    // The helper inserts occurred_at_ms=1000: closed time after death is 29s,
    // NOT current wall time minus 1970. Do not count the 20..30s offline gap.
    assert(reader.player_summary(custom.player_id,StatsCategory::Custom,custom,error));
    long long play=0,since=0;
    for(auto const& value:custom.rows){
        if(value.key=="minecraft:play_time")play=value.value;
        if(value.key=="minecraft:time_since_death")since=value.value;
    }
    assert(play==600 && since==580 && !custom.total_available);
    assert(reader.ranking(StatsCategory::Custom,"minecraft:play_time",0,1,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].display_name=="Alice" && custom_rank[0].value==600);
    assert(reader.ranking(StatsCategory::Custom,"minecraft:play_time",1,1,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].display_name=="Bob" && custom_rank[0].value==600);
    assert(reader.ranking(StatsCategory::Custom,"minecraft:time_since_death",0,10,custom_rank,error));
    assert(custom_rank.size()==2 && custom_rank[0].display_name=="Bob" && custom_rank[1].value==580);
    for(auto limit:{0u,101u})assert(!reader.ranking(StatsCategory::Mined,{},0,limit,custom_rank,error));
    assert(!reader.ranking(StatsCategory::Mined,{},100001,1,custom_rank,error));
    assert(!reader.player_summary("absent",StatsCategory::Custom,custom,error));

    // Numeric causes and old mixed-format death records are NOT killed_by.
    PlayerStatSummary killed_by;
    assert(reader.player_summary("uuid64:1:2",StatsCategory::KilledBy,killed_by,error));
    assert(killed_by.rows.empty() && killed_by.total==0);
    insert_killer(db,"uuid64:1:2","minecraft:zombie");
    insert_killer(db,"uuid64:1:2","minecraft:skeleton",kKillerChild,"child_projectile");
    insert_killer(db,"uuid64:3:4","minecraft:zombie");
    for(auto raw:{"null","[]","{bad","{}"})insert_killer(db,"uuid64:1:2","minecraft:zombie",raw);
    insert_killer(db,"uuid64:1:2","cause:5",kKillerDirect,"cause:5");
    insert_killer(db,"uuid64:1:2","a/b:bad");insert_killer(db,"uuid64:1:2","bad:x:y");
    insert_killer(db,"uuid64:1:2","minecraft:zombie",kKillerDirect,"child_projectile");
    insert_killer(db,"uuid64:1:2","minecraft:skeleton",kKillerChild,"direct");
    std::string duplicate=kKillerDirect;duplicate.pop_back();duplicate+=",\"self\":false}";
    insert_killer(db,"uuid64:1:2","minecraft:zombie",duplicate.c_str());
    for(auto patch:{"'$.after_dead',json('false')","'$.after_dead',1","'$.nested',json('true')",
                   "'$.same_level',json('false')","'$.source_id',0","'$.source_id',json('true')",
                   "'$.damaging_id',1","'$.damage_cause',3","'$.source_block',json('true')",
                   "'$.self',json('true')","'$.world_death_verified',json('true')"}) {
        insert_killer(db,"uuid64:1:2","minecraft:zombie");
        exec(db,(std::string("UPDATE events SET details_json=json_set(details_json,")+patch+
            ") WHERE event_id=last_insert_rowid()").c_str());
    }
    assert(reader.player_summary(killed_by.player_id,StatsCategory::KilledBy,killed_by,error));
    assert(killed_by.total==2 && killed_by.rows.size()==2 && killed_by.rows[0].key=="minecraft:skeleton");
    assert(reader.ranking(StatsCategory::KilledBy,{},0,1,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].display_name=="Alice" && custom_rank[0].value==2);
    assert(reader.ranking(StatsCategory::KilledBy,{},1,1,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].display_name=="Bob" && custom_rank[0].value==1);
    event(db,"direct_kill_observation","ll26.60.direct_attack_dead_transition.v1","minecraft:zombie","mob",1,"uuid64:1:2");
    event(db,"direct_kill_observation","ll26.60.projectile_owner_dead_transition.v1","minecraft:skeleton","mob",1,"uuid64:1:2");
    assert(reader.player_summary("uuid64:1:2",StatsCategory::Killed,killed_by,error));
    assert(killed_by.total==2 && killed_by.rows.size()==2);
    assert(reader.ranking(StatsCategory::Custom,"minecraft:mob_kills",0,10,custom_rank,error));
    assert(custom_rank.size()==1 && custom_rank[0].value==2);

    exec(db,"WITH RECURSIVE n(i) AS (VALUES(0) UNION ALL SELECT i+1 FROM n WHERE i<252) "
        "INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type) "
        "SELECT 1000,'item_pickup_delta','player','uuid64:1:2','ll26.60.take_inventory_delta.v1',1,"
        "printf('minecraft:fixture_%03d',i) FROM n");
    PlayerStatSummary paged;
    assert(reader.player_summary("uuid64:1:2",StatsCategory::PickedUp,paged,error,200,12));
    assert(paged.total==253 && paged.row_count==253 && paged.rows.size()==12);
    assert(paged.rows.front().key=="minecraft:fixture_200" && paged.rows.back().key=="minecraft:fixture_211");
    assert(reader.player_summary(paged.player_id,StatsCategory::PickedUp,paged,error,252,12));
    assert(paged.rows.size()==1 && paged.rows[0].key=="minecraft:fixture_252" && paged.total==253);
    assert(reader.player_summary(paged.player_id,StatsCategory::PickedUp,paged,error,300,12));
    assert(paged.rows.empty() && paged.row_count==253 && paged.total==253 && paged.total_available);
    assert(!reader.player_summary("uuid64:1:2",StatsCategory::PickedUp,paged,error,0,0));
    assert(!reader.player_summary("uuid64:1:2",StatsCategory::PickedUp,paged,error,100001,1));
    assert(reader.player_summary("uuid64:3:4",StatsCategory::PickedUp,paged,error,0,12));
    assert(!paged.total_available && paged.rows.empty());
    // CUSTOM time rows must join the same sorted page, not be appended to every
    // page. The union of tiny pages equals the default complete CUSTOM rows.
    assert(reader.player_summary("uuid64:1:2",StatsCategory::Custom,custom,error));
    for(std::size_t i=0;i<custom.row_count;++i) {
        assert(reader.player_summary("uuid64:1:2",StatsCategory::Custom,paged,error,i,1));
        assert(paged.rows.size()==1 && paged.rows[0].key==custom.rows[i].key && paged.rows[0].value==custom.rows[i].value);
    }

    PlayerStatSummary unsupported;
    assert(!reader.player_summary("uuid64:1:2", StatsCategory::Unknown, unsupported, error));

    reader.close();
    sqlite3_close(db);
    std::filesystem::remove(path, ignored);
    std::cout << "StatsReader PASS: player lookup, category totals, custom mapping, play_time, ranking, paging\n";
}
