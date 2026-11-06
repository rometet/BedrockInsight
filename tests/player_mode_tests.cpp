#include "bedrock_insight/core/mode_observation.hpp"
#include "bedrock_insight/levilamina/player_modes.hpp"
#include "fake_sdk.hpp"
#include <iostream>
#include <stdexcept>
using namespace bedrock_insight;
using namespace bedrock_insight::levilamina;
void require(bool ok){if(!ok)throw std::runtime_error("mode assertion");}
void collect(void* context, AnalyticsEvent value){static_cast<std::vector<AnalyticsEvent>*>(context)->push_back(std::move(value));}
int main() {
 try {
    for(bool before:{false,true})for(bool wanted:{false,true})for(bool actual:{false,true}) {
        PendingMode p;p.note(before,wanted,0);
        require(!p.observe(actual,249999));
        require(p.observe(actual,250000).has_value()==(before!=wanted && wanted==actual));
        require(!p.observe(actual,500000));
    }
    PendingMode p;p.note(false,true,1000000);p.note(true,true,1100000);
    require(p.observe(true,1250000).has_value()); // duplicate request retains first baseline
    p.note(false,true,1000000);p.note(true,false,1100000);require(!p.observe(false,1250000));
    p.note(false,true,0);p.note(false,true,1900000);require(!p.observe(true,2000001)); // duplicates don't extend TTL
    p.note(false,true,1000000);require(!p.observe(true,999999));
    require(jst_observation_day(53999999)==0 && jst_observation_day(54000000)==1);
    require(!jst_observation_day(-1));
    Level level,other;Player player;player.level=&level;
    std::vector<AnalyticsEvent> rows;ModeEventSink sink{&rows,&collect};
    auto tracker=make_mode_tracker();
    note_mode(*tracker,player,PlayerMode::Sprint,true,1000000);player.sprinting=true;
    poll_modes(*tracker,level,1250000,53999999,sink);require(rows.empty());
    note_mode(*tracker,player,PlayerMode::Sprint,false,1300000);player.sprinting=false;
    poll_modes(*tracker,level,1550000,54000000,sink); // first day's start emitted before new-day stop
    require(rows.size()==1 && rows[0].secondary_type=="start" && rows[0].occurred_at_ms==53999999);
    finish_modes(*tracker,sink);
    require(rows.size()==2 && rows[1].secondary_type=="stop" && rows[1].occurred_at_ms==54000000);
    finish_modes(*tracker,sink);require(rows.size()==2); // idempotent stop, no pending inferred
    tracker=make_mode_tracker();rows.clear();player.sneaking=false;
    note_mode(*tracker,player,PlayerMode::Sneak,true,1000000);player.sneaking=true;
    poll_modes(*tracker,other,1250000,1000,sink);require(rows.empty());
    poll_modes(*tracker,level,1500000,1100,sink);
    forget_modes(*tracker,"uuid64:1:2",sink);require(rows.size()==1 && rows[0].primary_type=="sneak");
    tracker=make_mode_tracker();rows.clear();player.sprinting=false;
    note_mode(*tracker,player,PlayerMode::Sprint,true,1000000);player.sprinting=true;player.valid=false;
    poll_modes(*tracker,level,1250000,1000,sink);finish_modes(*tracker,sink);require(rows.empty());
    player.valid=true;
    tracker=make_mode_tracker();note_mode(*tracker,player,PlayerMode::Sprint,false,1000000);
    poll_modes(*tracker,level,1250000,1000,sink); // mismatch, request alone doesn't count
    finish_modes(*tracker,sink);require(rows.empty());
    tracker=make_mode_tracker();player.sprinting=false;
    for(int i=0;i<300;++i){player.uuid={static_cast<unsigned long long>(i+1),2};note_mode(*tracker,player,PlayerMode::Sprint,true,1000000);}
    player.sprinting=true;poll_modes(*tracker,level,1250000,1000,sink);
    poll_modes(*tracker,level,6250000,6000,sink);require(rows.size()==256); // hard capacity, only observed states
    std::cout<<"Player mode PASS: delayed flags/coalescing/TTL/JST/flush/disconnect/removed/mismatch/capacity; fake SDK, no BDS.\n";
    return 0;
 }catch(std::exception const& e){std::cerr<<"Mode FAIL: "<<e.what()<<'\n';return 1;}
}
