#include "bedrock_insight/core/absorption_observation.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace bedrock_insight;
AbsorptionOutcome outcome() {return {AbsorptionHook::Player,8000,6000,4000,0,2,1,true,true,true,false};}
DamageAttributionFacts facts(bool child=false) {return {child ? 3 : 2,true,child,false,100,child ? 200 : 100,true,true,false};}
int main() {
    assert(absorption_milli(1.125f)==1125 && absorption_milli(0)==0);
    assert(!absorption_milli(-1) && !absorption_milli(100001));
    assert(!absorption_milli(std::numeric_limits<float>::infinity()));
    assert(!absorption_milli(std::numeric_limits<float>::quiet_NaN()));
    auto good=outcome();assert(absorption_amount(good)==20);
    for(int i=0;i<15;++i){auto bad=good;switch(i){
        case 0:bad.result_type=0;break;case 1:bad.result_type=1;break;case 2:bad.same_actor=false;break;
        case 3:bad.same_attribute=false;break;case 4:bad.same_level=false;break;case 5:bad.ambiguous=true;break;
        case 6:bad.parent_calls=2;break;case 7:bad.hook=AbsorptionHook::Mob;break;case 8:bad.before=100000001;break;
        case 9:bad.after=-1;break;case 10:bad.after=bad.before;break;case 11:bad.input=0;break;
        case 12:bad.input=1000;break;case 13:bad.returned=-1;break;case 14:bad.returned=100000001;break;
    }assert(absorption_amount(bad)==0);}
    auto tiny=good;tiny.after=tiny.before-49;assert(absorption_amount(tiny)==0);
    tiny.after=tiny.before-50;assert(absorption_amount(tiny)==1);
    int identity=0,other=0;
    AbsorptionCall c{&identity,AbsorptionHook::Player};assert(c.admit_parent(&identity,AbsorptionHook::Mob));
    assert(c.parent_calls==1 && !c.ambiguous);assert(!c.admit_parent(&identity,AbsorptionHook::Mob) && c.ambiguous);
    for(int mode=0;mode<3;++mode){AbsorptionCall bad{&identity,mode==0 ? AbsorptionHook::Mob : AbsorptionHook::Player};
        assert(!bad.admit_parent(mode==1 ? &other : &identity,mode==2 ? AbsorptionHook::Player : AbsorptionHook::Mob));assert(bad.ambiguous);}
    int origins=0,emits=0;
    auto run=[&](bool prep,bool post,bool sink,bool engine){return observe_absorption(
        [&]()->std::optional<int>{if(prep)throw std::runtime_error("pre");return 1;},
        [&]{++origins;if(engine)throw std::runtime_error("engine");return 37;},
        [&](int&,int result,auto& emit){assert(result==37);if(post)throw std::runtime_error("post");emit(1);},
        [&](int){++emits;if(sink)throw std::runtime_error("sink");});};
    assert(run(false,false,false,false)==37);assert(run(true,false,false,false)==37);
    assert(run(false,true,false,false)==37);assert(run(false,false,true,false)==37);
    try{run(false,false,false,true);assert(false);}catch(std::runtime_error const& e){assert(std::string(e.what())=="engine");}
    assert(origins==5 && emits==2);
    auto path=std::filesystem::temp_directory_path()/"absorption_fixture.sqlite3";assert(!std::filesystem::exists(path));
    Actor alice{ActorKind::Player,"uuid64:1:2","Alice"},bob{ActorKind::Player,"uuid64:3:4","Bob"};
    StorageWriter writer({path,64});assert(writer.start());constexpr std::int64_t at=1793890801000;
    for(bool dealt:{false,true}) {
        auto type=dealt ? "minecraft:zombie" : "minecraft:player";
        auto o=good;if(dealt){o.hook=AbsorptionHook::Mob;o.parent_calls=0;}
        auto event=absorption_event(alice,type,dealt,o,facts(),at);assert(event);assert(writer.submit(*event));
        auto small=o;small.after=7000;event=absorption_event(alice,type,dealt,small,facts(true),at+1);assert(event);assert(writer.submit(*event));
        event=absorption_event(bob,type,dealt,o,facts(),at+2);assert(event);assert(writer.submit(*event));
        auto bad=*event;bad.details_json="{";assert(writer.submit(bad));
        bad=*event;bad.amount=21;assert(writer.submit(bad));
        bad=*event;bad.secondary_type="minecraft:pig";assert(writer.submit(bad));
        bad=*event;bad.details_json.pop_back();bad.details_json+=",\"same_actor\":true}";assert(writer.submit(bad));
    }
    auto unknown=facts();unknown.resolved=false;assert(!absorption_event(alice,"minecraft:pig",true,good,unknown,at));
    auto self=facts();self.self=true;assert(!absorption_event(alice,"minecraft:player",true,good,self,at));
    assert(writer.flush());writer.stop();
    StatsReader reader(path);std::string error;assert(reader.open(error));PlayerStatSummary personal;
    assert(reader.player_summary(alice.id,StatsCategory::Custom,personal,error));
    for(auto key:{"minecraft:damage_absorbed","minecraft:damage_dealt_absorbed"}) {
        bool found=false;for(auto const& r:personal.rows)if(r.key==key){assert(r.value==30);found=true;}assert(found);
        std::vector<RankingEntry> ranks;assert(reader.ranking(StatsCategory::Custom,key,0,10,ranks,error));
        assert(ranks.size()==2 && ranks[0].value==30 && ranks[1].value==20);
    }
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"Absorption PASS: delta/parent/attribution/origin once/writer/SQLite/rank/integrity; synthetic, not engine proof\n";
}
