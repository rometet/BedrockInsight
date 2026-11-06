// Runtime lifecycle seam ONLY; never an engine inventory/hook test or DLL source.
#include "bedrock_insight/levilamina/player_pickup.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class PickupCapture final {
public:
    explicit PickupCapture(PickupSink fn):sink(std::move(fn)){}
    PickupSink sink;std::atomic<bool> active{true};
};
namespace {std::weak_ptr<PickupCapture> current;}
bool test_pickup_install_failure=false;
PickupToken start_pickup_capture(PickupSink sink) noexcept {
    if(test_pickup_install_failure || !current.expired())return {};
    try {auto token=std::make_shared<PickupCapture>(std::move(sink));current=token;return token;}catch(...){return {};}
}
void stop_pickup_capture(PickupToken const& token) noexcept {
    if(!token)return;
    token->active=false;if(current.lock()==token)current.reset();
}
PickupSink test_pickup_callback(){if(auto token=current.lock())return token->sink;return {};}
}
