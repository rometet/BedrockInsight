#include "bedrock_insight/core/resistance_observation.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace bedrock_insight;
ResistanceOutcome good(){return {AbsorptionHook::Player,10000,8000,5000,4000,1,600,2,1,1,true,true,true,true,false};}
DamageAttributionFacts facts(bool child=false){return {child?3:2,true,child,false,100,child?200:100,true,true,false};}
int main(){
    auto o=good();assert(resistance_amount(o)==20);
    for(int a=0;a<=255;++a){auto v=o;v.amplifier=a;assert(resistance_amount(v)==10*std::min(a+1,5));}
    for(int mode=0;mode<23;++mode){auto b=o;switch(mode){
        case 0:b.result_type=0;break;case 1:b.result_type=1;break;case 2:b.same_actor=false;break;
        case 3:b.same_level=false;break;case 4:b.same_source=false;break;case 5:b.same_effect=false;break;
        case 6:b.ambiguous=true;break;case 7:b.stage_calls=0;break;case 8:b.stage_calls=2;break;
        case 9:b.parent_calls=2;break;case 10:b.hook=AbsorptionHook::Mob;break;case 11:b.input=0;break;
        case 12:b.input=100000001;break;case 13:b.stage_input=10002;break;case 14:b.basis=8002;break;
        case 15:b.basis=0;break;case 16:b.returned=-1;break;case 17:b.returned=100000001;break;
        case 18:b.amplifier=-1;break;case 19:b.amplifier=256;break;case 20:b.duration=0;break;
        case 21:b.duration=-1;break;case 22:b.parent_calls=-1;break;
    }assert(resistance_amount(b)==0);}
    auto tiny=o;tiny.amplifier=0;tiny.basis=249;assert(resistance_amount(tiny)==0);
    tiny.basis=250;assert(resistance_amount(tiny)==1);
    auto full=o;full.amplifier=255;full.input=full.stage_input=full.basis=100000000;assert(resistance_amount(full)==1000000);
    int a=0,s=0,other=0;
    ResistanceStageGate gate{&a,&s};assert(gate.enter(&a,&s));assert(!gate.enter(&a,&s) && gate.ambiguous && gate.calls==2);
    for(int mode=0;mode<3;++mode){ResistanceStageGate g{&a,&s};
        if(mode==2){assert(g.enter(&a,&s));g.active=false;}
        assert(!g.enter(mode==0?&other:&a,mode==1?&other:&s) && g.ambiguous);}
    int origins=0,finishes=0;
    auto invoke=[&](bool prep,bool post,bool engine){return observe_resistance(
        [&]()->std::optional<int>{if(prep)throw std::runtime_error("pre");return 1;},
        [&]{++origins;if(engine)throw std::runtime_error("engine");return 1.25f;},
        [&](int&,float r){++finishes;assert(r==1.25f);if(post)throw std::runtime_error("post");});};
    assert(invoke(false,false,false)==1.25f);assert(invoke(true,false,false)==1.25f);assert(invoke(false,true,false)==1.25f);
    try{invoke(false,false,true);assert(false);}catch(std::runtime_error const& e){assert(std::string(e.what())=="engine");}
    assert(origins==4 && finishes==2);
    auto path=std::filesystem::temp_directory_path()/"resistance_fixture.sqlite3";assert(!std::filesystem::exists(path));
    Actor alice{ActorKind::Player,"uuid64:1:2","Alice"},bob{ActorKind::Player,"uuid64:3:4","Bob"};
    StorageWriter writer({path,64});assert(writer.start());constexpr std::int64_t at=1793890801000;
    for(bool dealt:{false,true}){
        auto type=dealt?"minecraft:zombie":"minecraft:player";auto v=o;if(dealt){v.hook=AbsorptionHook::Mob;v.parent_calls=0;}
        auto event=resistance_event(alice,type,dealt,v,facts(),at);assert(event && writer.submit(*event));
        v.basis=2500;event=resistance_event(alice,type,dealt,v,facts(true),at+1);assert(event && writer.submit(*event));
        v.basis=5000;event=resistance_event(bob,type,dealt,v,facts(),at+2);assert(event && writer.submit(*event));
        auto bad=*event;bad.details_json="{";assert(writer.submit(bad));
        bad=*event;bad.amount=21;assert(writer.submit(bad));
        bad=*event;bad.details_json.pop_back();bad.details_json+=",\"same_source\":true}";assert(writer.submit(bad));
        bad=*event;bad.secondary_type="minecraft:pig";assert(writer.submit(bad));
    }
    auto unknown=facts();unknown.resolved=false;assert(!resistance_event(alice,"minecraft:pig",true,o,unknown,at));
    assert(writer.flush());writer.stop();StatsReader reader(path);std::string error;assert(reader.open(error));PlayerStatSummary personal;
    assert(reader.player_summary(alice.id,StatsCategory::Custom,personal,error));
    for(auto key:{"minecraft:damage_resisted","minecraft:damage_dealt_resisted"}){
        bool found=false;for(auto const& r:personal.rows)if(r.key==key){assert(r.value==30);found=true;}assert(found);
        std::vector<RankingEntry> rank;assert(reader.ranking(StatsCategory::Custom,key,0,10,rank,error));
        assert(rank.size()==2 && rank[0].value==30 && rank[1].value==20);
    }
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"Resistance PASS: stage correlation/formula/cap/round/origin once/writer/SQLite/rank/integrity; synthetic only\n";
}
