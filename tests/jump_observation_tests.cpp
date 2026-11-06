#include "bedrock_insight/core/jump_observation.hpp"
#include "bedrock_insight/core/state_transition.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace bedrock_insight;
void require(bool v){if(!v)throw std::runtime_error("jump boundary assertion");}
int main(){try{
    JumpState before{-80000,0,true,false,false},after{420000,0,false,false,false};
    require(observed_upward_impulse(before,after,true,false));
    after.grounded=true;require(observed_upward_impulse(before,after,true,false)); // no fake airborne claim
    require(!observed_upward_impulse(before,after,false,false));
    require(!observed_upward_impulse(before,after,true,true));
    for(int bit=0;bit<7;++bit){auto b=before,a=after;
        if(bit==0)b.grounded=false;if(bit==1)b.vehicle=true;if(bit==2)a.vehicle=true;
        if(bit==3)b.dead=true;if(bit==4)a.dead=true;if(bit==5)a.dimension=1;if(bit==6)b.y_micro=16000001;
        require(!observed_upward_impulse(b,a,true,false));
    }
    for(auto y:{-16000001LL,-80000LL,0LL,16000001LL}){auto a=after;a.y_micro=y;require(!observed_upward_impulse(before,a,true,false));}
    before.y_micro=420000;require(!observed_upward_impulse(before,after,true,false)); // repeated call, no new increase
    require(jump_y_micro(0.42)==420000 && jump_y_micro(-0.08)==-80000);
    require(jump_y_micro(16)==16000000 && jump_y_micro(-16)==-16000000);
    require(!jump_y_micro(16.01) && !jump_y_micro(-16.01));
    require(!jump_y_micro(std::numeric_limits<double>::infinity()) && !jump_y_micro(std::numeric_limits<double>::quiet_NaN()));
    for(int failure=0;failure<4;++failure){int calls=0,posts=0,emits=0;bool engine_error=false;
        try {observe_state_transition([&]()->std::optional<int>{if(failure==0)throw 1;return 1;},
            [&]{++calls;if(failure==1)throw 7;},[&](int&){++posts;if(failure==2)throw 2;return true;},
            [&](int){++emits;if(failure==3)throw 3;});}catch(int e){engine_error=e==7;}
        require(calls==1 && engine_error==(failure==1));
        require(posts==((failure==0||failure==1)?0:1) && emits==(failure==3?1:0));
    }
    std::cout<<"Jump boundary PASS: direct-state quantization, ground/mount/death/dimension/identity/nested/bounds, repeated no-op, origin/exception; no BDS.\n";
    return 0;
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
