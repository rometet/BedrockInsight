// Lifecycle seam ONLY. Never engine jump/physics proof or native DLL code.
#include "bedrock_insight/levilamina/player_jump.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class JumpCapture final {
public:
    explicit JumpCapture(JumpSink fn):sink(std::move(fn)){}
    JumpSink sink;std::atomic<bool> active{true};
};
namespace {std::weak_ptr<JumpCapture> current;}
bool test_jump_install_failure=false;
JumpToken start_jump_capture(JumpSink sink) noexcept {
    if(test_jump_install_failure || !current.expired())return {};
    try{auto token=std::make_shared<JumpCapture>(std::move(sink));current=token;return token;}catch(...){return {};}
}
void stop_jump_capture(JumpToken const& token) noexcept {
    if(!token)return;
    token->active=false;if(current.lock()==token)current.reset();
}
JumpSink test_jump_callback(){if(auto token=current.lock())return token->sink;return {};}
}
