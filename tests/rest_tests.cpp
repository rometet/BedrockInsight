#include "bedrock_insight/core/rest_counter.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace bedrock_insight;
int main(){
    RestCounter c;assert(c.observe(false,0) && !c.known && c.ticks==0);
    for(int i=1;i<=100;++i)assert(!c.observe(false,i*1000));
    assert(!c.known);assert(c.observe(true,5000000) && c.known && c.ticks==0);
    for(int i=1;i<100;++i)assert(!c.observe(false,5000000+i*50000));
    assert(c.observe(false,10000000) && c.ticks==100);
    assert(!c.observe(false,10050000) && c.ticks==101);
    assert(!c.observe(true,10100000) && c.ticks==0);
    assert(c.observe(false,50000001) && !c.known && c.ticks==0);
    assert(c.observe(false,10) && !c.known);assert(!c.observe(false,-1));
    assert(c.observe(true,5000010) && c.known && c.ticks==0);
    c.ticks=INT64_MAX;assert(!c.observe(false,5000011) && !c.known && c.ticks==0);
    int origins=0,observations=0;
    observe_rest_tick([&]{++origins;},[&]{++observations;throw std::runtime_error("observe");});
    try{observe_rest_tick([&]{++origins;throw std::runtime_error("origin");},[&]{++observations;});assert(false);}
    catch(std::runtime_error const& e){assert(std::string(e.what())=="origin");}
    assert(origins==2 && observations==1);
    auto path=std::filesystem::temp_directory_path()/"rest_fixture.sqlite3";assert(!std::filesystem::exists(path));
    StorageWriter writer({path,64});assert(writer.start());constexpr std::int64_t at=1793890801000;
    Actor alice{ActorKind::Player,"uuid64:1:2","Alice"},bob{ActorKind::Player,"uuid64:3:4","Bob"},charlie{ActorKind::Player,"uuid64:5:6","Charlie"};
    auto put=[&](Actor const& actor,std::int64_t ticks,bool known,std::int64_t offset){
        RestCounter state;state.ticks=ticks;state.known=known;
        auto v=rest_snapshot(actor,state,at+offset);assert(v);assert(writer.submit(*v));return *v;};
    put(alice,100,true,0);put(alice,200,true,1);put(alice,0,true,2);
    put(bob,120,true,0);auto good=put(bob,200,true,2);
    put(charlie,300,true,0);put(charlie,0,false,2);
    auto bad=good;bad.occurred_at_ms=at+3;assert(writer.submit(bad)); // JSON timestamp mismatch
    bad=good;bad.details_json="{";assert(writer.submit(bad));
    bad=good;bad.details_json.pop_back();bad.details_json+=",\"ticks\":200}";assert(writer.submit(bad));
    RestCounter unknown;unknown.ticks=1;assert(!rest_snapshot(alice,unknown,at));
    assert(writer.flush());writer.stop();
    StatsReader reader(path);std::string error;assert(reader.open(error));
    auto value=[&](std::string const& id)->std::optional<std::int64_t>{PlayerStatSummary s;
        assert(reader.player_summary(id,StatsCategory::Custom,s,error));
        for(auto const& r:s.rows)if(r.key=="minecraft:time_since_rest")return r.value;return {};};
    assert(value(alice.id)==0 && value(bob.id)==200 && !value(charlie.id));
    std::vector<RankingEntry> ranks;assert(reader.ranking(StatsCategory::Custom,"minecraft:time_since_rest",0,10,ranks,error));
    assert(ranks.size()==1 && ranks[0].value==200);
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"Rest PASS: completed ticks/unknown/reset/gap/overflow/origin once/writer/latest SQLite/rank; synthetic only\n";
}
