#include "bedrock_insight/core/direct_kill.hpp"
#include "bedrock_insight/core/state_transition.hpp"
#include "bedrock_insight/core/damage_attribution.hpp"
#include <optional>
#include <stdexcept>
#include <iostream>
using namespace bedrock_insight;
void require(bool v){if(!v)throw std::runtime_error("direct kill assertion");}
int main(){try{
    DamageAttributionFacts attribution{2,true,false,false,-19,-19,true,true,false};
    require(resolved_damage_path(attribution)=="direct");
    auto child=attribution;child.cause=3;child.child=true;child.damaging_id=99;
    require(resolved_damage_path(child)=="child_projectile");
    for(int change=0;change<11;++change){auto f=attribution;
        if(change==0)f.cause=5;if(change==1)f.entity=false;if(change==2)f.child=true;
        if(change==3)f.block=true;if(change==4)f.source_id=0;if(change==5)f.damaging_id=0;
        if(change==6)f.resolved=false;if(change==7)f.same_level=false;if(change==8)f.self=true;
        if(change==9)f.damaging_id=99;if(change==10)f.cause=3;
        require(resolved_damage_path(f).empty());
    }
    child.damaging_id=child.source_id;require(resolved_damage_path(child).empty());
    DirectAttackFacts valid{2,true,false,false,12,12,true,false};require(credible_direct_attack(valid));
    // Projectile cause and a child projectile owner are attributable kills.
    DirectAttackFacts bow{3,true,true,false,12,13,true,false};require(credible_direct_attack(bow));
    DirectAttackFacts direct_proj{3,true,false,false,12,12,true,false};require(!credible_direct_attack(direct_proj));
    bow.damaging_id=0;require(!credible_direct_attack(bow));
    for(int change=0;change<8;++change){auto f=valid;
        if(change==0)f.cause=4;if(change==1)f.entity=false;if(change==2)f.block=true;
        if(change==3)f.source_id=0;if(change==4)f.damaging_id=13;if(change==5)f.resolved_player=false;
        if(change==6)f.self=true;if(change==7)f.cause=-1;
        require(!credible_direct_attack(f));
    }
    // A non-child source with mismatched ids is never credited to the shooter.
    auto mismatch=valid;mismatch.damaging_id=13;require(!credible_direct_attack(mismatch));
    valid.source_id=-99;valid.damaging_id=-99;require(credible_direct_attack(valid)); // no guessed positive-ID range
    for(bool before:{false,true})for(bool after:{false,true})for(bool same:{false,true})
    for(bool nested:{false,true})for(bool active:{false,true})
        require(direct_death_transition(before,after,same,nested,active)==(!before&&after&&same&&!nested&&active));
    bool dead=false;int count=0,calls=0;
    auto run=[&]{observe_state_transition([&]()->std::optional<bool>{return dead?std::nullopt:std::optional<bool>{dead};},
        [&]{++calls;dead=true;},[&](bool& before){return direct_death_transition(before,dead,true,false,true);},[&](bool){++count;});};
    run();run();require(count==1 && calls==2);dead=false;run();require(count==2 && calls==3);
    for(int failure=0;failure<4;++failure){int origin=0,post=0;bool threw=false;
        try{observe_state_transition([&]()->std::optional<int>{if(failure==0)throw 1;return 1;},
            [&]{++origin;if(failure==1)throw 7;},[&](int&){++post;if(failure==2)throw 2;return true;},
            [&](int){if(failure==3)throw 3;});}catch(int e){threw=e==7;}
        require(origin==1 && threw==(failure==1) && post==((failure==0||failure==1)?0:1));
    }
    std::cout<<"Direct kill boundary PASS: source IDs/entity/player/self exclusions,32 transition cases,repeat/respawn,origin/exception; no BDS.\n";return 0;
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
