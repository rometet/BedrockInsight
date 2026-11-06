// Lifecycle seam ONLY, never linked into a real DLL. Not an engine/slot test.
#include "bedrock_insight/levilamina/player_hurt.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class HurtCapture final {
public:
    explicit HurtCapture(HurtSink fn):sink(std::move(fn)){}
    HurtSink sink;std::atomic<bool> active{true};
};
namespace{std::weak_ptr<HurtCapture> current;}
bool test_hurt_install_failure=false;
HurtToken start_hurt_capture(HurtSink sink) noexcept {
    if(test_hurt_install_failure || !current.expired())return {};
    try{auto token=std::make_shared<HurtCapture>(std::move(sink));current=token;return token;}catch(...){return {};}
}
void stop_hurt_capture(HurtToken const& token) noexcept {
    if(!token)return;
    token->active=false;if(current.lock()==token)current.reset();
}
HurtSink test_hurt_callback(){if(auto token=current.lock())return token->sink;return {};}
}
