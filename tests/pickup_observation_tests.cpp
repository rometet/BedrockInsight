#include "bedrock_insight/core/pickup_observation.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace bedrock_insight;
void require(bool b){if(!b)throw std::runtime_error("pickup boundary assertion");}
int main(){try{
    require(pickup_inventory_gain(64,36,36,0,64,true)==64);
    require(pickup_inventory_gain(64,36,36,63,64,true)==1); // partial inventory increase, not entry64
    require(pickup_inventory_gain(255,128,128,32000,32255,true)==255);
    for(auto before:{-1LL,9181LL,std::numeric_limits<long long>::max()})
        require(!pickup_inventory_gain(64,36,36,before,10,true));
    for(auto after:{-1LL,0LL,63LL,129LL,9181LL,std::numeric_limits<long long>::max()})
        require(!pickup_inventory_gain(64,36,36,64,after,true));
    for(int entry:{-1,0,256})require(!pickup_inventory_gain(entry,36,36,0,1,true));
    for(int slots:{-1,0,129})require(!pickup_inventory_gain(1,slots,slots,0,1,true));
    require(!pickup_inventory_gain(64,36,35,0,1,true));
    require(!pickup_inventory_gain(64,36,36,0,1,false));
    for(bool accepted:{false,true})for(bool snap:{false,true})for(bool post:{false,true}) {
        int origins=0,posts=0,emits=0;
        bool result=observe_pickup_result([&]()->std::optional<int>{return snap?std::optional<int>{4}:std::nullopt;},
            [&]{++origins;return accepted;},[&](int& entry){require(entry==4);++posts;return post;},
            [&](int entry){require(entry==4);++emits;});
        require(result==accepted && origins==1 && posts==(accepted&&snap) && emits==(accepted&&snap&&post));
    }
    for(int failure=0;failure<3;++failure) {
        int origins=0;
        require(observe_pickup_result([&]()->std::optional<int>{if(failure==0)throw 1;return 4;},
            [&]{++origins;return true;},[&](int&){if(failure==1)throw 2;return true;},
            [&](int){if(failure==2)throw 3;}) && origins==1);
    }
    bool threw=false;int posts=0;
    try{observe_pickup_result([]{return std::optional<int>{4};},[]()->bool{throw 7;},
        [&](int&){++posts;return true;},[](int){});}catch(int e){threw=e==7;}
    require(threw && posts==0);
    std::cout<<"Pickup boundary PASS: partial/full net delta, bounds, changed container, true/false, origin once, exceptions; no BDS.\n";
    return 0;
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
