#include "bedrock_insight/core/state_transition.hpp"
#include <optional>
#include <iostream>
#include <stdexcept>
struct Entry {int identity=1;};
void require(bool ok) {if(!ok)throw std::runtime_error("transition assertion failed");}
int main() {
    try {
        int cases=0;
        for (bool before : {false,true}) for (bool after : {false,true})
        for (bool valid : {false,true}) for (bool same : {false,true}) for (bool active : {false,true}) {
            int origins=0,emitted=0,post=0;
            bedrock_insight::observe_state_transition([&]() -> std::optional<Entry> {
                if (before || !active) return {};return Entry{};
            },[&]{++origins;},[&](Entry const& e) {
                ++post;if(!valid || !same)return false;return e.identity==1 && after;
            },[&](Entry){++emitted;});
            require(origins==1 && emitted==(!before && active && valid && same && after));
            require(post==(!before && active));++cases;
        }
        // Two calls on an already-dead entity must not count twice. Respawn
        // clears the state; the next real transition may be counted again.
        bool dead=false;int emitted=0;
        auto invoke=[&] {
            bedrock_insight::observe_state_transition([&]() -> std::optional<Entry> {
                if(dead)return {};return Entry{};
            },[&]{dead=true;},[](Entry){return true;},[&](Entry){++emitted;});
        };
        invoke();invoke();require(emitted==1);dead=false;invoke();require(emitted==2);
        for (int failure=0;failure<4;++failure) {
            int origins=0,post=0,sinks=0;bool engine_threw=false;
            try {
                bedrock_insight::observe_state_transition([&]() -> std::optional<Entry> {
                    if(failure==0)throw std::bad_alloc();return Entry{};
                },[&]{++origins;if(failure==1)throw 42;},[&](Entry) {
                    ++post;if(failure==2)throw std::bad_alloc();return true;
                },[&](Entry){++sinks;if(failure==3)throw std::bad_alloc();});
            } catch (int value) {engine_threw=value==42;}
            require(origins==1 && engine_threw==(failure==1));
            require(post==(failure>=2) && sinks==(failure==3));
        }
        std::cout<<"State transition boundary PASS ("<<cases<<" combinations, duplicate/respawn, observer/engine exceptions). No BDS/weak-ref ABI proof.\n";
        return 0;
    } catch (...) {std::cerr<<"State transition boundary FAIL\n";return 1;}
}
