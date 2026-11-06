#include "bedrock_insight/core/use_observation.hpp"
#include "bedrock_insight/core/pickup_observation.hpp"
#include <cassert>
#include <optional>
#include <stdexcept>
#include <iostream>
using namespace bedrock_insight;
int main(){
    assert(use_inventory_loss(64,36,36,64,63,true)==1);
    assert(use_inventory_loss(1,36,36,0,0,true)==0); // e.g. offhand outside measured inventory
    assert(use_inventory_loss(1,36,36,1,2,true)==0); // increase is not consumption
    assert(use_inventory_loss(255,128,128,32640,32385,true)==255);
    for(auto entry:{0,256})assert(!use_inventory_loss(entry,36,36,64,63,true));
    for(auto size:{0,129})assert(!use_inventory_loss(1,size,size,1,0,true));
    assert(!use_inventory_loss(1,36,35,1,0,true));
    assert(!use_inventory_loss(1,36,36,1,0,false));
    assert(!use_inventory_loss(1,36,36,2,0,true));
    assert(!use_inventory_loss(1,36,36,-1,0,true));
    assert(!use_inventory_loss(1,36,36,0,-1,true));
    assert(!use_inventory_loss(1,36,36,9181,9180,true));
    assert(!use_inventory_loss(1,36,36,9180,9181,true));
    for(bool result:{false,true})for(bool entry:{false,true})for(bool valid:{false,true}){
        int calls=0,reads=0,emitted=0;
        auto got=observe_pickup_result([&]()->std::optional<int>{if(entry)return 1;return {};},
            [&]{++calls;return result;},[&](int&){++reads;return valid;},[&](int){++emitted;});
        assert(got==result && calls==1 && reads==int(result&&entry) && emitted==int(result&&entry&&valid));
    }
    int calls=0;
    assert(observe_pickup_result([]()->std::optional<int>{throw 1;},[&]{++calls;return true;},
        [](int&){assert(false);return true;},[](int){assert(false);}));
    assert(calls==1);
    assert(observe_pickup_result([]{return std::optional<int>(1);},[]{return true;},
        [](int&)->bool{throw 1;},[](int){assert(false);}));
    assert(observe_pickup_result([]{return std::optional<int>(1);},[]{return true;},
        [](int&){return true;},[](int){throw 1;}));
    try{observe_pickup_result([]{return std::optional<int>(1);},[]()->bool{throw std::runtime_error("engine");},
        [](int&){assert(false);return true;},[](int){assert(false);});assert(false);}
    catch(std::runtime_error const&){}
    std::cout<<"Use observation PASS: bounded deltas, zero is not consumption, bool/exception/origin-once; no engine proof\n";
}
