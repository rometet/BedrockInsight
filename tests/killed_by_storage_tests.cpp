#include "bedrock_insight/core/storage.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "killed_by_fixture.hpp"
#include <filesystem>
#include <iostream>

using namespace bedrock_insight;
int main() {
    auto const path=std::filesystem::temp_directory_path()/"killed_by_storage_fixture.sqlite3";
    assert(!std::filesystem::exists(path)); // runner supplies a fresh unique tmp
    {
        StorageWriter writer({path,32});assert(writer.start());
        AnalyticsEvent death;
        death.kind=EventKind::PlayerDeath;death.occurred_at_ms=1000;
        death.actor={ActorKind::Player,"uuid64:1:2","Alice"};
        death.cause="ll26.60.player_dead_transition.v1";
        death.primary_type="minecraft:player";death.secondary_type="cause:2";
        assert(writer.submit(death));
        auto a=death;a.kind=EventKind::PlayerKilledByEntity;
        a.cause="ll26.60.resolved_killer_dead_transition.v1";
        a.primary_type="minecraft:zombie";a.secondary_type="direct";a.details_json=kKillerDirect;
        assert(writer.submit(a));
        a.primary_type="minecraft:skeleton";a.secondary_type="child_projectile";a.details_json=kKillerChild;
        assert(writer.submit(a));
        assert(writer.flush());auto m=writer.metrics();
        assert(m.accepted_events==3 && m.committed_events==3 && m.dropped_events==0 && !m.failed);
        writer.stop();
    }
    std::string error;
    StatsReader reader(path);assert(reader.open(error));
    PlayerStatSummary summary;
    assert(reader.player_summary("uuid64:1:2",StatsCategory::KilledBy,summary,error));
    assert(summary.total==2 && summary.rows.size()==2);
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    auto scalar=[&](char const* sql) {
        sqlite3_stmt* s=nullptr;assert(sqlite3_prepare_v2(db,sql,-1,&s,nullptr)==SQLITE_OK);
        assert(sqlite3_step(s)==SQLITE_ROW);auto v=sqlite3_column_int64(s,0);sqlite3_finalize(s);return v;
    };
    assert(scalar("SELECT SUM(amount) FROM player_daily_types WHERE metric='killed_by_entities'")==2);
    assert(scalar("SELECT SUM(deaths) FROM player_daily")==1); // attribution is not a second death
    assert(scalar("SELECT SUM(kills) FROM player_daily")==0); // nor the victim's kill
    assert(scalar("PRAGMA user_version")==4);
    sqlite3_stmt* integrity=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&integrity,nullptr)==SQLITE_OK);
    assert(sqlite3_step(integrity)==SQLITE_ROW);
    assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(integrity,0)))=="ok");
    sqlite3_finalize(integrity);sqlite3_close(db);reader.close();
    std::cout<<"Killed-by storage PASS: actual bounded writer -> schema4 -> independent reader; no duplicate deaths; closed integrity ok; fixture only\n";
}
