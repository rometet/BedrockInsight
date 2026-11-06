// Lifecycle seam only, NOT damage-source/lookup/weak hook engine proof or DLL code.
#include "bedrock_insight/levilamina/player_kills.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class KillCapture final {
public:
    explicit KillCapture(KillSink fn):sink(std::move(fn)){}
    KillSink sink;std::atomic<bool> active{true};
};
namespace{std::weak_ptr<KillCapture> current;}
bool test_kill_install_failure=false;
KillToken start_kill_capture(KillSink sink) noexcept {
    if(test_kill_install_failure || !current.expired())return {};
    try{auto token=std::make_shared<KillCapture>(std::move(sink));current=token;return token;}catch(...){return {};}
}
void stop_kill_capture(KillToken const& token) noexcept {
    if(!token)return;
    token->active=false;if(current.lock()==token)current.reset();
}
KillSink test_kill_callback(){if(auto token=current.lock())return token->sink;return {};}
}
