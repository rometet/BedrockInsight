// Lifecycle seam ONLY for the fake-SDK test binary. Never built into the mod.
#include "bedrock_insight/levilamina/busy_tick.hpp"
#include "fake_sdk.hpp"
#include <atomic>

namespace bedrock_insight::levilamina {
class BusyTickState final {
public:
    explicit BusyTickState(BusyTickSink value):sink(std::move(value)) {}
    BusyTickSink sink;
    std::atomic<bool> active{true};
};
namespace { std::weak_ptr<BusyTickState> current; }
bool test_busy_install_failure=false;
BusyTickToken start_busy_ticks(BusyTickSink sink) noexcept {
    if(test_busy_install_failure || !current.expired()) return {};
    try { auto state=std::make_shared<BusyTickState>(std::move(sink)); current=state; return state; }
    catch (...) {return {};}
}
void stop_busy_ticks(BusyTickToken const& token) noexcept {
    if(!token)return;
    token->active=false;
    if(current.lock()==token)current.reset();
}
void test_busy_window(Level& level, BusyTickWindow const& window, bool valid) {
    if(auto state=current.lock();state && state->active) state->sink(level,window,valid);
}
}
