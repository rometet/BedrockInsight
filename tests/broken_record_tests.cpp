#include "broken_record_fixture.hpp"
#include "bedrock_insight/core/storage.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/remote_api.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace bedrock_insight;
#define CHECK(e) do{if(!(e))throw std::runtime_error("line "+std::to_string(__LINE__)+": " #e);}while(false)
namespace {
std::string replace(std::string s,std::string const& from,std::string const& to){auto p=s.find(from);CHECK(p!=std::string::npos);s.replace(p,from.size(),to);return s;}
std::string bytes(std::filesystem::path const& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(){try{
    Actor alice{ActorKind::Player,"private:broken:a","Alice<svg>"},bob{ActorKind::Player,"private:broken:b","Bob"},carol{ActorKind::Player,"private:broken:c","Carol"};
    BrokenObservation main{BrokenLocation::Inventory,36,0,100,99,2,1,0,0};
    auto make=[&](Actor const& a,BrokenObservation o,std::string type="minecraft:iron_pickaxe"){return broken_record(a,type,o,1793890801000);};
    auto valid=make(alice,main);CHECK(valid && valid->amount==1 && valid->secondary_type=="main_inventory");
    CHECK(valid_broken_record(valid->details_json,valid->primary_type,valid->cause,valid->secondary_type));
    CHECK(valid_broken_record(legacy_broken_details(),"minecraft:iron_pickaxe",kBrokenV1,""));
    CHECK(!valid_broken_record(legacy_broken_details(),"minecraft:iron_pickaxe",kBrokenV2,""));
    for(bool removed:{false,true})for(bool same:{false,true})for(bool level:{false,true})
        CHECK(broken_frame_valid(removed,same,level,0,0)==(!removed && same && level));
    for(int dim:{-2,1,128,256})CHECK(!broken_frame_valid(false,true,true,0,dim));
    for(int slot=0;slot<4;++slot){auto armor=main;armor.location=BrokenLocation::Armor;armor.slots=4;armor.slot=slot;
        auto v=make(alice,armor,"minecraft:iron_helmet");CHECK(v && valid_broken_record(v->details_json,v->primary_type,v->cause,"armor"));}
    for(int which=0;which<12;++which){auto bad=main;
        switch(which){case 0:bad.delta_damage=0;break;case 1:bad.entry_units=2;break;case 2:bad.after_units=1;break;
            case 3:bad.max_damage=1;break;case 4:bad.damage_before=2;break;case 5:bad.slot=36;break;
            case 6:bad.dimension=128;break;case 7:bad.location=BrokenLocation::Offhand;break;
            case 8:bad.location=BrokenLocation::Armor;break;case 9:bad.slots=129;break;
            case 10:bad.damage_before=-1;break;case 11:bad.max_damage=32768;break;}
        CHECK(!make(alice,bad));
    }
    auto path=std::filesystem::temp_directory_path()/"broken_record_fixture.sqlite3";CHECK(!std::filesystem::exists(path));
    StorageWriter writer({path,256});CHECK(writer.start());CHECK(writer.submit(*valid));
    auto shield=main;shield.location=BrokenLocation::Offhand;shield.slots=1;CHECK(writer.submit(*make(alice,shield,"minecraft:shield")));
    auto armor=main;armor.location=BrokenLocation::Armor;armor.slots=4;armor.slot=1;
    CHECK(writer.submit(*make(alice,armor,"minecraft:iron_helmet")));
    armor.slot=3;CHECK(writer.submit(*make(carol,armor,"minecraft:iron_boots")));
    auto legacy=*make(bob,main);legacy.cause=kBrokenV1;legacy.secondary_type.clear();legacy.details_json=legacy_broken_details();CHECK(writer.submit(legacy));
    for(auto patch:{std::pair{"\"same_player\":true","\"same_player\":false"},
        {"\"same_level\":true","\"same_level\":false"},{"\"same_slot\":true","\"same_slot\":false"},
        {"\"threshold_input_reached\":true","\"threshold_input_reached\":false"},
        {"\"final_break_verified\":false","\"final_break_verified\":true"},
        {"\"world_save_verified\":false","\"world_save_verified\":true"},
        {"\"damage_before\":99","\"damage_before\":1"},{"\"dimension\":0","\"dimension\":128"},
        {"\"slot\":0","\"slot\":false"},{"\"slot\":0","\"slot\":0.0"},
        {"\"nested\":false","\"nested\":false,\"nested\":false"},
        {"\"nested\":false","\"nested\":false,\"unknown\":true"}}){
        auto bad=*valid;bad.details_json=replace(bad.details_json,patch.first,patch.second);
        CHECK(!valid_broken_record(bad.details_json,bad.primary_type,bad.cause,bad.secondary_type));CHECK(writer.submit(std::move(bad)));
    }
    for(int which=0;which<8;++which){auto bad=*valid;
        switch(which){case 0:bad.amount=999;break;case 1:bad.primary_type="minecraft:diamond";break;
            case 2:bad.details_json="{}";break;case 3:bad.secondary_type="armor";break;
            case 4:bad.actor.kind=ActorKind::System;break;case 5:bad.occurred_at_ms=4102444800000LL;break;
            case 6:bad.kind=EventKind::ItemHurtSlotLoss;break;case 7:bad.cause="ll26.60.durability_item_destroyed.v3";break;}
        CHECK(writer.submit(std::move(bad)));
    }
    auto invalid=*valid;invalid.actor={ActorKind::Player,"private:broken:d","InvalidOnly"};invalid.details_json="{}";CHECK(writer.submit(invalid));
    if(!writer.flush())throw std::runtime_error("writer: "+writer.last_error());writer.stop();auto before=bytes(path);
    StatsReader reader(path);std::string error;CHECK(reader.open(error));PlayerStatSummary summary;
    CHECK(reader.player_summary(alice.id,StatsCategory::Broken,summary,error));CHECK(summary.total_available && summary.total==3 && summary.rows.size()==3);
    StatRankingPage page;CHECK(reader.ranking_page(StatsCategory::Broken,"",0,1,page,error));
    CHECK(page.total_entries==3 && page.has_more() && page.rows[0].value==3);
    CHECK(reader.ranking_page(StatsCategory::Broken,"",1,2,page,error));CHECK(page.rows.size()==2 && page.rows[0].display_name=="Bob" && page.rows[1].display_name=="Carol");
    CHECK(reader.ranking_page(StatsCategory::Broken,"",999,1,page,error));CHECK(page.total_entries==3 && page.rows.empty());
    CHECK(reader.player_summary(invalid.actor.id,StatsCategory::Broken,summary,error));CHECK(!summary.total_available && summary.rows.empty());reader.close();
    set_remote_api_database(path);char buffer[8192];CHECK(getPlayerStats("Alice<svg>","minecraft:broken",buffer,sizeof(buffer))>0);
    std::string json=buffer;CHECK(json.find("\"total\":3")!=std::string::npos && json.find(kBrokenV2)!=std::string::npos
        && json.find("\"final_world_commit_verified\":false")!=std::string::npos && json.find("private:broken:")==std::string::npos);
    CHECK(getPlayerStatsPage("Alice<svg>","broken",1,2,buffer,sizeof(buffer))>0);CHECK(std::string(buffer).find("\"total_entries\":3")!=std::string::npos);
    CHECK(getRankStats("broken",0,2,buffer,sizeof(buffer))>0);CHECK(getRankStatsPage("broken",1,2,buffer,sizeof(buffer))>0);
    CHECK(getPlayerStats("InvalidOnly","broken",buffer,sizeof(buffer))>0);CHECK(std::string(buffer).find("\"total\":null")!=std::string::npos);CHECK(bytes(path)==before);
    std::cout<<"Broken v2 PASS: owned inventory/offhand/armor model, frame/threshold/typed evidence, native reader/pages/C API, read-only; synthetic only\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
