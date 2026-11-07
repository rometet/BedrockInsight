#include "bedrock_insight/core/world_tick.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include "bedrock_insight/core/remote_api.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace bedrock_insight;
constexpr std::int64_t stamp = 1793890800000;
int main() {
    assert(!world_tick_snapshot(UINT64_MAX,stamp,"run-a"));
    assert(!world_tick_snapshot(1,-1,"run-a"));
    assert(!world_tick_snapshot(1,stamp,""));
    assert(!world_tick_snapshot(1,stamp,std::string(129,'a')));
    auto path=std::filesystem::temp_directory_path()/"world_tick_fixture.sqlite3";
    assert(!std::filesystem::exists(path));
    StorageWriter writer({path,64});assert(writer.start());
    for (auto const& actor : {Actor{ActorKind::Player,"uuid64:1:2","Alice"},Actor{ActorKind::Player,"uuid64:3:4","Bob"}}) {
        AnalyticsEvent join;join.kind=EventKind::SessionStart;join.actor=actor;join.occurred_at_ms=stamp-1000;
        join.session_id="session-"+actor.id;assert(writer.submit(join));
    }
    auto push=[&](std::uint64_t tick,std::int64_t at,char const* run) {
        auto value=world_tick_snapshot(tick,at,run);assert(value);
        assert(value->tick_mspt==0 && value->online_players==-1);
        assert(writer.submit(*value));
    };
    auto expect=[&](std::int64_t tick) {
        assert(writer.flush());StatsReader reader(path);std::string error;assert(reader.open(error));
        for(auto id:{"uuid64:1:2","uuid64:3:4"}) {
            PlayerStatSummary s;assert(reader.player_summary(id,StatsCategory::Custom,s,error));
            bool found=false;for(auto const& row:s.rows)if(row.key=="minecraft:total_world_time"){assert(row.value==tick);found=true;}
            assert(found && !s.total_available);
        }
        std::vector<RankingEntry> rank;
        assert(!reader.ranking(StatsCategory::Custom,"minecraft:total_world_time",0,10,rank,error));
        assert(rank.empty() && error.find("world value")!=std::string::npos);
    };
    push(1000000,stamp-1,"run-old");push(40,stamp+5000,"run-new");expect(40);
    push(0,stamp+10000,"run-newer");expect(0); // measured zero is not missing
    push(9,stamp+15000,"run-newer");
    auto invalid=*world_tick_snapshot(999,stamp+20000,"run-newer");
    invalid.details_json="{broken";assert(writer.submit(invalid));
    invalid=*world_tick_snapshot(999,stamp+20000,"run-newer");invalid.amount=998;assert(writer.submit(invalid));
    invalid=*world_tick_snapshot(999,stamp+20000,"run-newer");invalid.details_json.pop_back();
    invalid.details_json+=",\"tick_id\":999}";assert(writer.submit(invalid));
    invalid=*world_tick_snapshot(999,stamp+20000,"run-newer");invalid.actor.kind=ActorKind::Player;assert(writer.submit(invalid));
    expect(9);writer.stop();
    set_remote_api_database(path);char buffer[32768];
    assert(getPlayerStats("Alice","custom",buffer,sizeof(buffer))>0);
    std::string json(buffer);auto const begin=json.find("\"key\":\"minecraft:total_world_time\"");
    assert(begin!=std::string::npos);
    assert(json.substr(begin,json.find('}',begin)-begin+1).find("\"value\":9}")!=std::string::npos);
    set_remote_api_database({});
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);
    assert(sqlite3_prepare_v2(db,"SELECT COUNT(*) FROM server_tick_samples",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW && sqlite3_column_int64(check,0)==0);
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"World tick PASS: builder/writer -> SQLite/native/API; latest not SUM/MAX, real zero, shared not rankable; synthetic only\n";
}
