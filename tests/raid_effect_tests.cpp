#include "bedrock_insight/core/raid_effect.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace bedrock_insight;
RaidEffectOutcome outcome(int id) {
    RaidEffectOutcome o;o.requested=o.after={id,id==36 ? 600 : 48000,0};
    o.same_player=o.same_level=true;return o;
}
int main() {
    auto good=outcome(36);assert(valid_raid_effect(good));
    for(int test=0;test<12;++test){auto bad=good;
        switch(test){case 0:bad.requested.id=28;break;case 1:bad.requested.duration=599;break;
        case 2:bad.requested.amplifier=-1;break;case 3:bad.same_player=false;break;
        case 4:bad.same_level=false;break;case 5:bad.nested=true;break;
        case 6:bad.after.id=29;break;case 7:bad.after.duration=601;break;
        case 8:bad.after.amplifier=1;break;case 9:bad.before=bad.after;break;
        case 10:bad.before=RaidEffectState{36,100,256};break;
        case 11:bad.before=RaidEffectState{36,-2,0};break;}
        assert(!valid_raid_effect(bad));
    }
    good.before=RaidEffectState{36,100,0};assert(valid_raid_effect(good));
    int calls=0,emits=0;
    auto run=[&](bool prep,bool post,bool sink,bool engine) {
        observe_raid_effect([&]() -> std::optional<int>{if(prep)throw std::runtime_error("pre");return 1;},
            [&]{++calls;if(engine)throw std::runtime_error("engine");},
            [&](int&)->std::optional<int>{if(post)throw std::runtime_error("post");return 1;},
            [&](int){++emits;if(sink)throw std::runtime_error("sink");});
    };
    run(false,false,false,false);run(true,false,false,false);run(false,true,false,false);run(false,false,true,false);
    assert(calls==4 && emits==2);
    try{run(false,false,false,true);assert(false);}catch(std::runtime_error const& e){assert(std::string(e.what())=="engine");}
    assert(calls==5 && emits==2);
    auto path=std::filesystem::temp_directory_path()/"raid_effect_fixture.sqlite3";assert(!std::filesystem::exists(path));
    Actor alice{ActorKind::Player,"uuid64:1:2","Alice"},bob{ActorKind::Player,"uuid64:3:4","Bob"};
    StorageWriter writer({path,64});assert(writer.start());
    constexpr std::int64_t stamp=1793890801000;
    for(auto id:{36,29}){
        auto o=outcome(id);auto a=raid_effect_event(alice,o,stamp);assert(a);assert(writer.submit(*a));
        o.before=RaidEffectState{id,100,0};a=raid_effect_event(alice,o,stamp+1);assert(a);assert(writer.submit(*a));
        auto b=raid_effect_event(bob,o,stamp+2);assert(b);assert(writer.submit(*b));
        auto bad=*b;bad.details_json="{";assert(writer.submit(bad));
        bad=*b;bad.amount=2;assert(writer.submit(bad));
        bad=*b;bad.secondary_type="effect_id:28";assert(writer.submit(bad));
        bad=*b;bad.details_json.pop_back();bad.details_json+=",\"nested\":false}";assert(writer.submit(bad));
    }
    assert(writer.flush());writer.stop();
    StatsReader reader(path);std::string error;assert(reader.open(error));
    PlayerStatSummary personal;assert(reader.player_summary(alice.id,StatsCategory::Custom,personal,error));
    for(auto key:{"minecraft:raid_trigger","minecraft:raid_win"}){
        bool found=false;for(auto const& r:personal.rows)if(r.key==key){assert(r.value==2);found=true;}assert(found);
        std::vector<RankingEntry> ranks;assert(reader.ranking(StatsCategory::Custom,key,0,10,ranks,error));
        assert(ranks.size()==2 && ranks[0].value==2 && ranks[1].value==1);
    }
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"Raid effect PASS: changed-state gates/origin once/writer/SQLite/personal/rank/integrity; synthetic, not raid proof\n";
}
