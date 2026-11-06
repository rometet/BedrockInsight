#pragma once
#include <sqlite3.h>
#include <cassert>
#include <string>

inline constexpr char kKillerDirect[] = R"({"hook":"Player::$die","phase":"post_dead_transition","before_dead":false,"after_dead":true,"nested":false,"source_entity":true,"source_child":false,"source_block":false,"resolved_actor":true,"same_level":true,"self":false,"world_death_verified":false,"damage_cause":2,"source_id":-19,"damaging_id":-19})";
inline constexpr char kKillerChild[] = R"({"hook":"Player::$die","phase":"post_dead_transition","before_dead":false,"after_dead":true,"nested":false,"source_entity":true,"source_child":true,"source_block":false,"resolved_actor":true,"same_level":true,"self":false,"world_death_verified":false,"damage_cause":3,"source_id":-19,"damaging_id":99})";
inline void insert_killer(sqlite3* db, char const* actor, char const* type,
    char const* details=kKillerDirect,char const* path="direct") {
    sqlite3_stmt* s=nullptr;
    assert(sqlite3_prepare_v2(db,"INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,"
        "primary_type,secondary_type,details_json) VALUES(1000,'player_killed_by_entity','player',?,"
        "'ll26.60.resolved_killer_dead_transition.v1',1,?,?,?)",-1,&s,nullptr)==SQLITE_OK);
    for(int i=1;i<=4;++i) {
        auto text=i==1?actor:i==2?type:i==3?path:details;
        assert(sqlite3_bind_text(s,i,text,-1,SQLITE_TRANSIENT)==SQLITE_OK);
    }
    assert(sqlite3_step(s)==SQLITE_DONE);assert(sqlite3_finalize(s)==SQLITE_OK);
}
