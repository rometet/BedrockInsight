#include "bedrock_insight/core/fish_bucket.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace bedrock_insight;
FishBucketOutcome outcome() {
    FishBucketOutcome o;o.interaction_success=o.bucket_swing=o.target_retired=true;
    o.same_player=o.same_inventory=o.same_dimension=true;
    o.bucket_calls=1;o.slots_before=o.slots_after=36;o.before_units=2;o.after_units=3;return o;
}
int main() {
    auto good=outcome();assert(valid_fish_bucket_outcome(good));
    for(int test=0;test<15;++test){auto bad=good;
        switch(test){case 0:bad.interaction_success=false;break;case 1:bad.bucket_swing=false;break;
        case 2:bad.target_retired=false;break;case 3:bad.same_player=false;break;case 4:bad.same_inventory=false;break;
        case 5:bad.same_dimension=false;break;case 6:bad.nested=true;break;case 7:bad.bucket_calls=0;break;
        case 8:bad.bucket_calls=2;break;case 9:bad.slots_after=35;break;case 10:bad.slots_before=129;break;
        case 11:bad.after_units=2;break;case 12:bad.after_units=4;break;case 13:bad.before_units=-1;break;
        case 14:bad.before_units=std::numeric_limits<std::int64_t>::max();break;}
        assert(!valid_fish_bucket_outcome(bad));
    }
    int origins=0,emissions=0;
    auto run=[&](bool prepare_error,bool finish_error,bool emit_error,bool origin_error) {
        return observe_fish_interaction([&]() -> std::optional<int>{if(prepare_error)throw std::runtime_error("capture");return 1;},
            [&]{++origins;if(origin_error)throw std::runtime_error("engine");return 17;},
            [&](int&,int result)->std::optional<int>{assert(result==17);if(finish_error)throw std::runtime_error("post");return 1;},
            [&](int){++emissions;if(emit_error)throw std::runtime_error("queue");});
    };
    assert(run(false,false,false,false)==17 && origins==1 && emissions==1);
    assert(run(true,false,false,false)==17 && origins==2 && emissions==1);
    assert(run(false,true,false,false)==17 && origins==3 && emissions==1);
    assert(run(false,false,true,false)==17 && origins==4 && emissions==2);
    try{run(false,false,false,true);assert(false);}catch(std::runtime_error const& e){assert(std::string(e.what())=="engine");}
    assert(origins==5 && emissions==2);
    auto path=std::filesystem::temp_directory_path()/"fish_bucket_fixture.sqlite3";assert(!std::filesystem::exists(path));
    Actor alice{ActorKind::Player,"uuid64:1:2","Alice"},bob{ActorKind::Player,"uuid64:3:4","Bob"};
    constexpr std::int64_t stamp=1793890800000;
    StorageWriter writer({path,64});assert(writer.start());
    for(auto const& pair:kFishBucketTypes) {
        auto observed=good;observed.target_unavailable=pair.actor=="minecraft:axolotl";
        auto event=fish_bucket_event(alice,pair.actor,observed,stamp+1000);assert(event);
        assert(event->kind==EventKind::PlayerInteractionResult && event->amount==1);assert(writer.submit(*event));
    }
    auto event=fish_bucket_event(bob,"minecraft:salmon",good,stamp+2000);assert(event);assert(writer.submit(*event));
    for(auto bad:{"addon:salmon","minecraft:dolphin","minecraft:fishing_hook","minecraft:cod_fake"})assert(!fish_bucket_event(alice,bad,good,stamp));
    auto bad=*event;bad.details_json="{bad";assert(writer.submit(bad));
    bad=*event;bad.amount=2;assert(writer.submit(bad));
    bad=*event;bad.secondary_type="minecraft:cod";assert(writer.submit(bad));
    bad=*event;bad.details_json.pop_back();bad.details_json+=",\"nested\":false}";assert(writer.submit(bad));
    assert(writer.flush());writer.stop();
    StatsReader reader(path);std::string error;assert(reader.open(error));
    PlayerStatSummary personal;assert(reader.player_summary(alice.id,StatsCategory::Custom,personal,error));
    bool found=false;for(auto const& row:personal.rows)if(row.key=="minecraft:fish_caught"){assert(row.value==7);found=true;}
    assert(found);
    std::vector<RankingEntry> ranks;assert(reader.ranking(StatsCategory::Custom,"minecraft:fish_caught",0,10,ranks,error));
    assert(ranks.size()==2 && ranks[0].value==7 && ranks[1].value==1);
    sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
    sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
    assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
    sqlite3_finalize(check);sqlite3_close(db);
    std::cout<<"Fish bucket PASS: gates/exactly-once origin/builder/writer/SQLite/native ranking/integrity; synthetic only\n";
}
