#include "bedrock_insight/core/busy_tick.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace bedrock_insight;
void require(bool value,char const* message) {if(!value)throw std::runtime_error(message);}
int main() {
 try {
    int level=0,other=0;
    auto scenario=[&](std::int64_t period,std::int64_t busy,std::int64_t ticks,std::int64_t peak,int n) {
        BusyTickSampler sampler;
        require(!sampler.observe(&level,0,busy,ticks,peak),"first frame only primes");
        std::optional<BusyTickWindow> sample;
        for(int i=1;i<=n;++i) sample=sampler.observe(&level,i*period,i*period+busy,ticks,peak);
        require(sample.has_value(),"5 second window emitted");return *sample;
    };
    auto idle=scenario(50000000,2000000,1,2000000,100);
    require(idle.tps()==20.0 && idle.mspt()==2.0,"idle is 20TPS/2MSPT, NOT 50MSPT");
    auto lag=scenario(100000000,80000000,1,80000000,50);
    require(lag.tps()==10.0 && lag.mspt()==80.0,"overload 10TPS/80MSPT");
    auto warp=scenario(50000000,6000000,3,2000000,100);
    require(warp.tps()==60.0 && warp.mspt()==2.0 && warp.max_inner_ns==2000000,"warp counts inner ticks, no 20 cap");
    auto frozen=scenario(50000000,100000,0,0,100);
    require(frozen.tps()==0 && !frozen.mspt(),"freeze observes 0 ticks and undefined MSPT");
    require(!BusyTickWindow{}.mspt(),"empty is undefined not 0MSPT");
    for(int fault=0;fault<6;++fault) {
        BusyTickSampler sampler; static_cast<void>(sampler.observe(&level,0,1,1,1));
        if(fault==0)static_cast<void>(sampler.observe(&other,2,3,1,1));
        if(fault==1)static_cast<void>(sampler.observe(&level,0,2,1,1));
        if(fault==2)static_cast<void>(sampler.observe(&level,2,1,1,1));
        if(fault==3)static_cast<void>(sampler.observe(&level,2,3,1,2));
        if(fault==4)static_cast<void>(sampler.observe(&level,2,3,0,1));
        if(fault==5)static_cast<void>(sampler.observe(&level,2,3,1,1,false));
        require(!sampler.valid(),"mixed Level/clock/exception frame invalidates, no false data");
    }
    BusyTickSampler bounded;
    static_cast<void>(bounded.observe(&level,0,1,1,1));
    static_cast<void>(bounded.observe(&level,2,3,1000000,1));
    static_cast<void>(bounded.observe(&level,4,5,1,1));
    require(!bounded.valid(),"tick count overflow bound");
    BusyTickSampler pause;
    static_cast<void>(pause.observe(&level,0,1,1,1));
    require(!pause.observe(&level,31000000000,31000000001,1,1),"long pause discarded");
    require(pause.observe(&level,36000000000,36000000001,1,1).has_value(),"fresh window after pause");
    BusyTickSampler near_max;
    auto const limit=std::numeric_limits<std::int64_t>::max();
    static_cast<void>(near_max.observe(&level,limit-6000000000,limit-5999999999,1,1));
    require(near_max.observe(&level,limit-1000000000,limit-999999999,1,1).has_value(),"signed arithmetic near limit");
    std::cout<<"Busy tick math PASS: idle, lag, warp, freeze, bounds, pause, invalid/exception frames\n";
    return 0;
 }catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}
}
