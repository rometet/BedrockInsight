#include "block_record_fixture.hpp"
#include "bedrock_insight/core/storage.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/remote_api.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace bedrock_insight;
#undef assert
#define assert(expression) do {if(!(expression))throw std::runtime_error("check failed line "+std::to_string(__LINE__)+": " #expression);} while(false)
namespace {
std::string change(std::string s,std::string const& from,std::string const& to){auto p=s.find(from);assert(p!=std::string::npos);s.replace(p,from.size(),to);return s;}
void exec(sqlite3* db,char const* sql){if(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
std::string bytes(std::filesystem::path const& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(){try{
    Actor alice{ActorKind::Player,"private:block:a","Alice<svg>"},bob{ActorKind::Player,"private:block:b","Bob"},carol{ActorKind::Player,"private:block:c","Carol"};
    auto make=[&](Actor const& actor,bool placed=false,int hand=0){return block_transition_record(actor,placed,
        placed?"minecraft:air":"minecraft:stone",placed?"minecraft:stone":"minecraft:air",{12,64,-9,0},placed?hand:-1,false,1793890801000);};
    auto valid=make(alice);assert(valid && valid->amount==1 && valid->cause=="ll26.60.gamemode_destroy_block.v2");
    assert(valid->position.x==12 && valid->position.y==64 && valid->position.z==-9 && valid->position.dimension==0);
    assert(valid_block_record(valid->details_json,"mined",valid->primary_type,valid->cause,""));
    assert(valid_block_record(legacy_mined_details("minecraft:stone"),"mined","minecraft:stone","ll26.60.gamemode_destroy_block.v1",""));
    assert(!valid_block_record(legacy_mined_details("minecraft:stone"),"mined","minecraft:stone",valid->cause,""));
    assert(!valid_block_record(valid->details_json,"mined","minecraft:dirt",valid->cause,""));
    assert(!valid_block_record(valid->details_json,"mined",valid->primary_type,valid->cause,"off_hand"));
    assert(!valid_block_record(valid->details_json,"mined",valid->primary_type,"ll26.60.gamemode_destroy_block.v3",""));
    for(int hand:{0,1}){
        auto placed=make(alice,true,hand);assert(placed && valid_block_record(placed->details_json,"placed",placed->primary_type,placed->cause,placed->secondary_type));
        assert(!valid_block_record(placed->details_json,"placed",placed->primary_type,placed->cause,hand==0?"off_hand":"main_hand"));
    }
    for(bool removed:{false,true})for(bool same:{false,true})for(bool level:{false,true})for(int dim:{-2,-1,0,1,256}){
        int calls=0,reads=0,emits=0;
        bool expected=!removed && same && level && dim==0;
        assert(observe_block_outcome([]{return std::optional<int>(1);},[&]{++calls;return true;},[&](int&){
            if(!block_frame_confirmed(removed,same,level,0,dim))return false;++reads;return true;
        },[&](int){++emits;}));assert(calls==1 && reads==int(expected) && emits==int(expected));
    }
    assert(!block_transition_record(alice,false,"minecraft:stone","minecraft:stone",{0,64,0,0},-1,false,1));
    assert(!block_transition_record(alice,true,"minecraft:air","minecraft:stone",{0,64,0,0},2,false,1));
    assert(!block_transition_record(alice,false,"minecraft:stone","minecraft:air",{0,64,0,128},-1,false,1));
    assert(!block_transition_record(alice,false,"minecraft:stone","minecraft:air",{30000000,64,0,0},-1,false,1));
    auto path=std::filesystem::temp_directory_path()/"block_record_fixture.sqlite3";assert(!std::filesystem::exists(path));
    StorageWriter writer({path,256});assert(writer.start());
    assert(writer.submit(*valid));assert(writer.submit(*valid));
    auto legacy=block_transition_record(bob,false,"minecraft:dirt","minecraft:air",{12,64,-9,0},-1,false,1793890801000);assert(legacy);
    legacy->cause="ll26.60.gamemode_destroy_block.v1";legacy->details_json=legacy_mined_details("minecraft:dirt");assert(writer.submit(*legacy));
    auto dirt=block_transition_record(carol,false,"minecraft:dirt","minecraft:air",{12,64,-9,0},-1,false,1793890801000);assert(dirt && writer.submit(*dirt));
    for(auto patch:{std::pair{"\"same_player\":true","\"same_player\":false"},
        {"\"same_level\":true","\"same_level\":false"},{"\"dimension\":0","\"dimension\":true"},
        {"\"dimension\":0","\"dimension\":1.0"},{"\"dimension\":0","\"dimension\":256"},
        {"\"nested\":false","\"nested\":true"},{"\"world_save_verified\":false","\"world_save_verified\":true"},
        {"\"phase\":\"return_true_post_state\"","\"phase\":\"pre_notification\""},
        {"\"same_level\":true","\"same_level\":false,\"same_level\":true"},
        {"\"dimension\":0","\"dimension\":0,\"future\":0"}}){
        auto bad=*valid;bad.details_json=change(bad.details_json,patch.first,patch.second);
        assert(!valid_block_record(bad.details_json,"mined",bad.primary_type,bad.cause,""));assert(writer.submit(std::move(bad)));
    }
    for(int type=0;type<7;++type){auto bad=*valid;
        switch(type){case 0:bad.amount=999;break;case 1:bad.details_json="{}";break;
        case 2:bad.primary_type="minecraft:dirt";break;case 3:bad.secondary_type="main_hand";break;
        case 4:bad.actor.kind=ActorKind::System;break;case 5:bad.kind=EventKind::AuditBlockChange;break;
        case 6:bad.occurred_at_ms=9000000000000000LL;break;}
        assert(writer.submit(std::move(bad)));
    }
    auto placed=make(alice,true,1);assert(placed && writer.submit(*placed));
    auto invalid_only=*valid;invalid_only.actor={ActorKind::Player,"private:block:d","InvalidOnly"};invalid_only.details_json="{}";
    assert(writer.submit(std::move(invalid_only)));
    assert(writer.flush());writer.stop();
    sqlite3* db=nullptr;assert(sqlite3_open(path.string().c_str(),&db)==SQLITE_OK);
    exec(db,"INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,actor_name,world_id,dimension,x,y,z,primary_type,secondary_type,session_id,cause,amount,details_json,tick_mspt,online_players) SELECT occurred_at_ms,event_kind,actor_kind,actor_id,actor_name,world_id,dimension,x,y,z,primary_type,secondary_type,session_id,cause,1.25,details_json,tick_mspt,online_players FROM events ORDER BY event_id LIMIT 1");
    sqlite3_stmt* position=nullptr;assert(sqlite3_prepare_v2(db,"SELECT x,y,z,dimension FROM events ORDER BY event_id LIMIT 1",-1,&position,nullptr)==SQLITE_OK);
    assert(sqlite3_step(position)==SQLITE_ROW && sqlite3_column_int(position,0)==12 && sqlite3_column_int(position,1)==64
        && sqlite3_column_int(position,2)==-9 && sqlite3_column_int(position,3)==0);sqlite3_finalize(position);
    sqlite3_close(db);auto before=bytes(path);
    StatsReader reader(path);std::string error;assert(reader.open(error));PlayerStatSummary summary;
    assert(reader.player_summary(alice.id,StatsCategory::Mined,summary,error));assert(summary.total_available && summary.total==2 && summary.rows.size()==1);
    StatRankingPage rank;assert(reader.ranking_page(StatsCategory::Mined,"",0,1,rank,error));
    assert(rank.total_entries==3 && rank.has_more() && rank.rows.size()==1 && rank.rows[0].display_name==alice.display_name && rank.rows[0].value==2);
    assert(reader.ranking_page(StatsCategory::Mined,"",1,2,rank,error));assert(rank.rows.size()==2 && rank.rows[0].display_name=="Bob" && rank.rows[1].display_name=="Carol");
    assert(reader.ranking_page(StatsCategory::Mined,"",100,1,rank,error));assert(rank.total_entries==3 && rank.rows.empty() && !rank.has_more());
    assert(reader.player_summary("private:block:d",StatsCategory::Mined,summary,error));assert(!summary.total_available && summary.rows.empty());
    reader.close();set_remote_api_database(path);char buffer[8192];
    assert(getPlayerStats("Alice<svg>","mined",buffer,sizeof(buffer))>0);
    auto json=std::string(buffer);assert(json.find("\"total\":2")!=std::string::npos && json.find("\"final_world_commit_verified\":false")!=std::string::npos);
    assert(json.find("ll26.60.gamemode_destroy_block.v2")!=std::string::npos && json.find("private:block:")==std::string::npos);
    assert(getRankStatsPage("mined",0,1,buffer,sizeof(buffer))>0);assert(std::string(buffer).find("\"total_entries\":3")!=std::string::npos);
    assert(getPlayerStats("InvalidOnly","mined",buffer,sizeof(buffer))>0);assert(std::string(buffer).find("\"total\":null")!=std::string::npos);
    assert(bytes(path)==before);
    std::cout<<"Block records PASS: v2 builder/frame guards, v1 history, strict malformed/attempt/future exclusion, writer/native/C API/paging/read-only; synthetic only\n";
}catch(std::exception const& error){std::cerr<<error.what()<<'\n';return 1;}}
