#pragma once
#include "bedrock_insight/core/broken_record.hpp"
#include <sqlite3.h>
inline std::string legacy_broken_details(){return R"({"hook":"ItemStackBase::hurtAndBreak","phase":"return_true_item_destroyed","scope":"main_inventory_same_slot","delta_damage":2,"slots":36,"slot":0,"max_damage":100,"damage_before":99,"is_damageable":true,"entry_units":1,"after_units":0,"nested":false,"durability_path":true,"durability_exhausted_verified":true,"max_damage_value_exceeded_read":true})";}
inline bool insert_broken_fixture(sqlite3* db,char const* id,char const* type){
    sqlite3_stmt* stmt=nullptr;
    if(sqlite3_prepare_v2(db,"INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type,secondary_type,details_json) VALUES(1000,'item_broken_result','player',?,'ll26.60.durability_item_destroyed.v1',1,?,'',?)",-1,&stmt,nullptr)!=SQLITE_OK)return false;
    auto raw=legacy_broken_details();
    bool ok=sqlite3_bind_text(stmt,1,id,-1,SQLITE_TRANSIENT)==SQLITE_OK
        && sqlite3_bind_text(stmt,2,type,-1,SQLITE_TRANSIENT)==SQLITE_OK
        && sqlite3_bind_text(stmt,3,raw.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK && sqlite3_step(stmt)==SQLITE_DONE;
    sqlite3_finalize(stmt);return ok;
}
