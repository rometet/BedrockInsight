// Lifecycle seam only, NOT GameMode/virtual inventory/weak engine proof or DLL code.
#include "bedrock_insight/levilamina/player_use.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class UseCapture final {
public:
    explicit UseCapture(UseSink fn):sink(std::move(fn)){}
    UseSink sink;std::atomic<bool> active{true};
};
namespace{std::weak_ptr<UseCapture> current;}
bool test_use_install_failure=false;
UseToken start_use_capture(UseSink sink) noexcept {
    if(test_use_install_failure || !current.expired())return {};
    try{auto token=std::make_shared<UseCapture>(std::move(sink));current=token;return token;}catch(...){return {};}
}
void stop_use_capture(UseToken const& token) noexcept {
    if(!token)return;
    token->active=false;if(current.lock()==token)current.reset();
}
UseSink test_use_callback(){if(auto token=current.lock())return token->sink;return {};}
}
