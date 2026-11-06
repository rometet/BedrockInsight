#include "bedrock_insight/core/drop_observation.hpp"
#include <iostream>
#include <stdexcept>
using namespace bedrock_insight;
void require(bool ok) {if(!ok)throw std::runtime_error("drop observation assertion");}
int main() {
    try {
        for (auto name : {"minecraft:stone","addon:item/path-1.2","a:b"}) require(valid_item_type(name));
        for (auto name : {"",":a","minecraft:","minecraft:a:b","mine/craft:a","Minecraft:a","a:<script>"}) require(!valid_item_type(name));
        require(!valid_item_type(std::string(129,'a')));
        for (bool random : {false,true}) for (bool success : {false,true}) {
            int snapshots=0,origins=0,emissions=0,count=64;
            auto result=observe_drop_result(random,[&]() -> std::optional<AnalyticsEvent> {
                ++snapshots;AnalyticsEvent e;e.amount=count;e.primary_type="minecraft:stone";return e;
            },[&] {++origins;count=0;return success;},[&](AnalyticsEvent e) {
                ++emissions;require(e.amount==64 && e.primary_type=="minecraft:stone");
            });
            require(result==success && origins==1 && snapshots==!random && emissions==(!random && success));
        }
        for (bool snapshot_fails : {false,true}) {
            int origins=0,emissions=0;
            require(observe_drop_result(false,[&]() -> std::optional<AnalyticsEvent> {
                if(snapshot_fails)throw std::bad_alloc();return {};
            },[&] {++origins;return true;},[&](AnalyticsEvent){++emissions;}));
            require(origins==1 && emissions==0);
        }
        int emitted=0;
        require(observe_drop_result(false,[] {return std::optional{AnalyticsEvent{}};},[] {return true;},
            [&](AnalyticsEvent){++emitted;throw std::bad_alloc();}));
        require(emitted==1);
        bool engine_exception=false;
        try {
            observe_drop_result(false,[] {return std::optional{AnalyticsEvent{}};},
                []() -> bool {throw 42;},[&](AnalyticsEvent){++emitted;});
        } catch (int code) {engine_exception=code==42;}
        require(engine_exception && emitted==1);
        std::cout<<"Drop result boundary PASS: original exactly once; failed/random/invalid snapshots excluded; pre-origin quantity; exceptions preserved. NOT engine proof.\n";
        return 0;
    } catch (...) {std::cerr<<"Drop result boundary FAIL\n";return 1;}
}
