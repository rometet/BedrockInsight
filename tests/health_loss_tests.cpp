#include "bedrock_insight/core/health_loss_observation.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/remote_api.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace bedrock_insight;
HealthLossOutcome health() {return {AbsorptionHook::Player,20000,17000,5000,3000,2,1,true,true,true,false};}
DamageAttributionFacts source(bool child=false) {return {child ? 3 : 2,true,child,false,100,child ? 200 : 100,true,true,false};}
int main() {
    auto good=health();assert(absorption_amount(good)==30);
    for(int n=0;n<15;++n){auto bad=good;switch(n){
        case 0:bad.result_type=0;break;case 1:bad.result_type=1;break;case 2:bad.same_actor=false;break;
        case 3:bad.same_attribute=false;break;case 4:bad.same_level=false;break;case 5:bad.ambiguous=true;break;
        case 6:bad.parent_calls=2;break;case 7:bad.hook=AbsorptionHook::Mob;break;case 8:bad.after=-1;break;
        case 9:bad.after=bad.before;break;case 10:bad.after=bad.before+1;break;case 11:bad.input=1;break;
        case 12:bad.returned=-1;break;case 13:bad.returned=100000001;break;case 14:bad.before=100000001;break;
    }assert(!health_loss_event({ActorKind::Player,"a","Alice"},"minecraft:player",false,bad,source(),0));}
    auto fatal=good;fatal.after=0;fatal.input=25000;assert(absorption_amount(fatal)==200);
    auto tiny=good;tiny.after=tiny.before-49;assert(absorption_amount(tiny)==0);
    tiny.after=tiny.before-50;assert(absorption_amount(tiny)==1);
    // The shared origin wrapper preserves exactly-once and exceptions even if
    // sampling or submission fails. The native adapter uses this wrapper.
    int calls=0,emits=0;
    for(int mode=0;mode<4;++mode) {
        auto result=observe_absorption([&]()->std::optional<int>{if(mode==1)throw 1;return 1;},
            [&]{++calls;return 17;},[&](int&,int r,auto& emit){assert(r==17);if(mode==2)throw 2;emit(1);},
            [&](int){++emits;if(mode==3)throw 3;});assert(result==17);
    }
    try{observe_absorption([]{return std::optional<int>(1);},[&]()->int{++calls;throw 4;},
        [](int&,int,auto&){},[](int){});assert(false);}catch(int e){assert(e==4);}
    assert(calls==5 && emits==2);
    auto path=std::filesystem::temp_directory_path()/"health_loss_fixture.sqlite3";
    assert(!std::filesystem::exists(path));StorageWriter writer({path,64});assert(writer.start());
    Actor alice{ActorKind::Player,"uuid64:1:2","Alice"},bob{ActorKind::Player,"uuid64:3:4","Bob"};
    constexpr std::int64_t at=1793890801000;
    for(bool dealt:{false,true}) {
        auto o=good;if(dealt){o.hook=AbsorptionHook::Mob;o.parent_calls=0;}
        auto type=dealt ? "minecraft:zombie" : "minecraft:player";
        auto event=health_loss_event(alice,type,dealt,o,source(),at);assert(event && writer.submit(*event));
        auto smaller=o;smaller.after=18000;event=health_loss_event(alice,type,dealt,smaller,source(true),at+1);
        assert(event && writer.submit(*event));
        event=health_loss_event(bob,type,dealt,o,source(),at+2);assert(event && writer.submit(*event));
        auto bad=*event;bad.details_json="{bad";assert(writer.submit(bad));
        bad=*event;bad.amount=31;assert(writer.submit(bad));
        bad=*event;bad.details_json.pop_back();bad.details_json+=",\"same_actor\":true}";assert(writer.submit(bad));
        bad=*event;bad.secondary_type="minecraft:pig";assert(writer.submit(bad));
        bad=*event;bad.kind=dealt ? EventKind::PlayerDamageDealt : EventKind::PlayerDamageTaken;
        bad.cause=dealt ? "ll26.60.actor_hurt_real_damage.v1" : "ll26.60.actor_hurt_event.v1";
        bad.amount=999;assert(writer.submit(bad));
        auto absorbed=absorption_event(bob,type,dealt,o,source(),at);assert(absorbed && writer.submit(*absorbed));
    }
    auto unknown=source();unknown.resolved=false;
    assert(!health_loss_event(alice,"minecraft:pig",true,good,unknown,at));
    auto self=source();self.self=true;assert(!health_loss_event(alice,"minecraft:player",true,good,self,at));
    assert(writer.flush());writer.stop();
    StatsReader reader(path);std::string error;assert(reader.open(error));PlayerStatSummary personal;
    set_remote_api_database(path);
    assert(reader.player_summary(alice.id,StatsCategory::Custom,personal,error));
    for(auto key:{"minecraft:damage_taken","minecraft:damage_dealt"}) {
        bool found=false;for(auto const& r:personal.rows)if(r.key==key){assert(r.value==50);found=true;}assert(found);
        StatRankingPage rank;assert(reader.ranking_page(StatsCategory::Custom,key,0,1,rank,error));
        assert(rank.rows.size()==1 && rank.rows[0].value==50 && rank.total_entries==2 && rank.has_more());
        assert(reader.ranking_page(StatsCategory::Custom,key,1,1,rank,error));
        assert(rank.rows.size()==1 && rank.rows[0].value==30 && !rank.has_more());
        char buffer[8192];assert(getPlayerStats("Alice","custom",buffer,sizeof(buffer))>0);
        assert(std::string(buffer).find(key)!=std::string::npos && std::string(buffer).find("uuid64:")==std::string::npos);
        auto category=std::string("custom:")+key;
        assert(getRankStatsPage(category.c_str(),0,1,buffer,sizeof(buffer))>0);
        assert(std::string(buffer).find("\"value\":50")!=std::string::npos);
        assert(std::string(buffer).find("\"total_entries\":2")!=std::string::npos);
    }
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"Health loss PASS: same-attribute delta, guards/rounding/origin once, writer/SQLite/personal/rank; notifications and absorption NOT mixed; synthetic only\n";
}
