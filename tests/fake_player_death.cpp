// Test binary lifecycle seam only, NOT engine/weak-ref verification.
#include "bedrock_insight/levilamina/player_death.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class DeathCapture final {
public:
    explicit DeathCapture(DeathSink fn):sink(std::move(fn)) {}
    DeathSink sink;
    std::atomic<bool> active{true};
};
namespace {std::weak_ptr<DeathCapture> current;}
bool test_death_install_failure=false;
DeathToken start_death_capture(DeathSink sink) noexcept {
    if(test_death_install_failure || !current.expired())return {};
    try {auto token=std::make_shared<DeathCapture>(std::move(sink));current=token;return token;}
    catch (...) {return {};}
}
void stop_death_capture(DeathToken const& token) noexcept {
    if(!token)return;
    token->active=false;
    if(current.lock()==token)current.reset();
}
DeathSink test_death_callback() {
    if(auto token=current.lock())return token->sink;
    return {};
}
}
