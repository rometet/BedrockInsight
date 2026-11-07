// Test-only core DLL setup. NEVER compiled into the BDS mod; no engine/LL
// dependency or DllMain. Only the owned test process calls this entry point.
#include "bedrock_insight/core/remote_api.hpp"
#include "block_record_fixture.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <utility>

extern "C" __declspec(dllexport) int __cdecl bi_test_create_database(wchar_t const* path) noexcept {
    if(path==nullptr)return -1;
    sqlite3* db=nullptr;
    try {
        auto file=std::filesystem::path(path);
        if(std::filesystem::exists(file))return -1; // never overwrite a DB
        if(sqlite3_open(file.string().c_str(),&db)!=SQLITE_OK) {
            if(db)sqlite3_close(db);
            return -1;
        }
        int code=sqlite3_exec(db,
            "CREATE TABLE players(player_id TEXT PRIMARY KEY,last_name TEXT,first_seen_ms INTEGER,last_seen_ms INTEGER);"
            "CREATE TABLE events(event_id INTEGER PRIMARY KEY,occurred_at_ms INTEGER,event_kind TEXT,actor_kind TEXT,"
            "actor_id TEXT,cause TEXT,amount INTEGER,primary_type TEXT,secondary_type TEXT,details_json TEXT,session_id TEXT);"
            "CREATE TABLE sessions(session_id TEXT PRIMARY KEY,player_id TEXT,started_at_ms INTEGER,ended_at_ms INTEGER,duration_ms INTEGER);"
            "INSERT INTO players VALUES('private:id1','Alice',0,1),('private:id2','Bob',0,1),"
            "('private:id3','a\"b\\c'||char(10),0,1),('private:id4','NoData',0,1);",
            nullptr,nullptr,nullptr);
        if(code==SQLITE_OK && (!insert_mined_fixture(db,"private:id1","minecraft:stone",3)
            || !insert_mined_fixture(db,"private:id2","minecraft:dirt",1)
            || !insert_mined_fixture(db,"private:id3","minecraft:stone",1)))code=SQLITE_ERROR;
        sqlite3_close(db);db=nullptr;
        if(code!=SQLITE_OK)return -1;
        bedrock_insight::set_remote_api_database(std::move(file));
        return 0;
    }catch(...){if(db)sqlite3_close(db);return -1;}
}
