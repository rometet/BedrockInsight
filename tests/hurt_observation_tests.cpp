#include "bedrock_insight/core/hurt_observation.hpp"
#include "bedrock_insight/core/pickup_observation.hpp"
#include <cassert>
#include <climits>
#include <iostream>
#include <optional>
#include <stdexcept>
using namespace bedrock_insight;
int main(){
    assert(hurt_slot_became_empty(1,1,0,36,36,0,true,true));
    assert(hurt_slot_became_empty(INT_MAX,1,0,128,128,127,true,true));
    for(int delta:{INT_MIN,-1,0})assert(!hurt_slot_became_empty(delta,1,0,36,36,0,true,true));
    for(int count:{0,2,255})assert(!hurt_slot_became_empty(1,count,0,36,36,0,true,true));
    for(int count:{-1,1,255})assert(!hurt_slot_became_empty(1,1,count,36,36,0,true,true));
    for(int size:{0,129})assert(!hurt_slot_became_empty(1,1,0,size,size,0,true,true));
    assert(!hurt_slot_became_empty(1,1,0,36,35,0,true,true));
    for(int slot:{-1,36})assert(!hurt_slot_became_empty(1,1,0,36,36,slot,true,true));
    assert(!hurt_slot_became_empty(1,1,0,36,36,0,false,true));
    assert(!hurt_slot_became_empty(1,1,0,36,36,0,true,false));
    for(bool result:{false,true})for(bool entry:{false,true})for(bool valid:{false,true}){
        int calls=0,reads=0,emits=0;
        assert(observe_pickup_result([&]()->std::optional<int>{if(entry)return 1;return {};},
            [&]{++calls;return result;},[&](int&){++reads;return valid;},[&](int){++emits;})==result);
        assert(calls==1 && reads==int(result&&entry) && emits==int(result&&entry&&valid));
    }
    int calls=0;
    assert(observe_pickup_result([]()->std::optional<int>{throw 1;},[&]{++calls;return true;},
        [](int&){assert(false);return true;},[](int){assert(false);}));assert(calls==1);
    assert(observe_pickup_result([]{return std::optional<int>(1);},[]{return true;},
        [](int&)->bool{throw 1;},[](int){assert(false);}));
    assert(observe_pickup_result([]{return std::optional<int>(1);},[]{return true;},
        [](int&){return true;},[](int){throw 1;}));
    try{observe_pickup_result([]{return std::optional<int>(1);},[]()->bool{throw std::runtime_error("engine");},
        [](int&){assert(false);return true;},[](int){assert(false);});assert(false);}
    catch(std::runtime_error const&){}
    std::cout<<"Hurt slot predicate/origin-once/exception PASS; NOT engine break proof\n";
}
