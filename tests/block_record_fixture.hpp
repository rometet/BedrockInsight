#pragma once
#include "bedrock_insight/core/block_record.hpp"
#include <sqlite3.h>
// Test-owned DB only. Exactly one durable row per modeled transition.
inline std::string legacy_mined_details(std::string_view item) {
    return std::string(R"({"hook":"GameMode::$destroyBlock","phase":"return_true_post_state","scope":"same_position_block_source","before_type":")")+
        std::string(item)+R"(","after_type":"minecraft:air","dimension":0,"nested":false,"state_transition_verified":true,"world_save_verified":false})";
}
inline bool insert_mined_fixture(sqlite3* db,std::string_view player,std::string_view item,int count,bool legacy=false) {
    using namespace bedrock_insight;
    if(count<1 || count>100)return false;
    auto value=block_transition_record({ActorKind::Player,std::string(player),""},false,item,"minecraft:air",{12,64,-9,0},-1,false,1793890801000);
    if(!value)return false;
    if(legacy){value->cause="ll26.60.gamemode_destroy_block.v1";value->details_json=legacy_mined_details(item);}
    sqlite3_stmt* statement=nullptr;
    if(sqlite3_prepare_v2(db,"INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type,secondary_type,details_json) VALUES(1793890801000,'block_mined_result','player',?,?,1,?,'',?)",-1,&statement,nullptr)!=SQLITE_OK)return false;
    auto bind=[&](int n,std::string_view text){return sqlite3_bind_text(statement,n,text.data(),static_cast<int>(text.size()),SQLITE_TRANSIENT)==SQLITE_OK;};
    bool ok=bind(1,player) && bind(2,value->cause) && bind(3,item) && bind(4,value->details_json);
    for(int i=0;i<count && ok;++i){ok=sqlite3_step(statement)==SQLITE_DONE;if(ok)ok=sqlite3_reset(statement)==SQLITE_OK;}
    sqlite3_finalize(statement);return ok;
}
