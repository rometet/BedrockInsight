#include "bedrock_insight/core/shield_evidence.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace bedrock_insight;
int main(){
    assert(shield_evidence_opt_in("1"));assert(!shield_evidence_opt_in(nullptr));
    for(auto raw:{"","0","01","true","1 "})assert(!shield_evidence_opt_in(raw));
    ShieldEvidenceRate rate;assert(!rate.admit("",0) && !rate.admit("p",-1) && !rate.admit(std::string(129,'x'),0));
    assert(rate.admit("p",0));assert(!rate.admit("p",4999999));assert(rate.admit("p",5000000));
    assert(!rate.admit("p",5000001));assert(rate.admit("p",1));
    ShieldEvidenceRate bounded;for(int i=0;i<256;++i)assert(bounded.admit(std::to_string(i),0));
    assert(!bounded.admit("overflow",1));assert(!bounded.admit("overflow",30000000));
    assert(bounded.admit("overflow",30000001));
    int origins=0,finishes=0;
    auto invoke=[&](bool prep,bool post,bool engine){return observe_shield_evidence(
        [&]()->std::optional<int>{if(prep)throw std::runtime_error("pre");return 1;},
        [&]{++origins;if(engine)throw std::runtime_error("engine");return 27;},
        [&](int&,int result){++finishes;assert(result==27);if(post)throw std::runtime_error("post");});};
    assert(invoke(false,false,false)==27 && invoke(true,false,false)==27 && invoke(false,true,false)==27);
    try{invoke(false,false,true);assert(false);}catch(std::runtime_error const& e){assert(std::string(e.what())=="engine");}
    assert(origins==4 && finishes==2);
    Actor player{ActorKind::Player,"uuid64:1:2","Alice"};ShieldEvidence e;e.input=6000;e.may_block=true;
    auto v=shield_evidence_event(player,e,1793890801000);assert(v && v->amount==0);
    assert(v->kind==EventKind::PlayerShieldEvidence && to_string(v->kind)=="player_shield_evidence");
    auto path=std::filesystem::temp_directory_path()/"shield_evidence_fixture.sqlite3";assert(!std::filesystem::exists(path));
    StorageWriter writer({path,64});assert(writer.start());assert(writer.submit(*v));
    // Hypothetical witnesses, NOT a declaration that any of these means block.
    e.after.blocked_flag=true;e.after.shield_time=100;v=shield_evidence_event(player,e,1793890802000);assert(v && writer.submit(*v));
    e.result_type=2;e.returned_present=true;e.returned=4000;v=shield_evidence_event(player,e,1793890803000);assert(v && writer.submit(*v));
    e.ambiguous=true;v=shield_evidence_event(player,e,1793890804000);assert(v && writer.submit(*v));
    auto bad=e;bad.result_type=3;assert(!shield_evidence_event(player,bad,1));
    bad=e;bad.input=-1;assert(!shield_evidence_event(player,bad,1));
    bad=e;bad.returned_present=false;assert(!shield_evidence_event(player,bad,1));
    bad=e;bad.input.reset();bad.returned.reset();assert(shield_evidence_event(player,bad,1));
    assert(writer.flush());writer.stop();StatsReader reader(path);std::string error;assert(reader.open(error));
    PlayerStatSummary stats;assert(reader.player_summary(player.id,StatsCategory::Custom,stats,error));assert(stats.rows.empty());
    std::vector<RankingEntry> ranks;assert(!reader.ranking(StatsCategory::Custom,"minecraft:damage_blocked_by_shield",0,10,ranks,error));
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    auto scalar=[&](char const* sql){sqlite3_stmt* s=nullptr;assert(sqlite3_prepare_v2(db,sql,-1,&s,nullptr)==SQLITE_OK);
        assert(sqlite3_step(s)==SQLITE_ROW);auto n=sqlite3_column_int64(s,0);sqlite3_finalize(s);return n;};
    assert(scalar("SELECT COUNT(*) FROM events WHERE event_kind='player_shield_evidence' AND amount=0")==4);
    assert(scalar("SELECT COUNT(*) FROM player_daily_types")==0);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"Shield evidence PASS: bound/rate/origin once/raw writer/SQLite/integrity; NEVER shield statistics, synthetic only\n";
}
