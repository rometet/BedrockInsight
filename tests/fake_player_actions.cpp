// Lifecycle seam ONLY. Not engine/hook verification and never built into DLL.
#include "bedrock_insight/levilamina/player_actions.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class DropCapture final {
public:
    explicit DropCapture(DropSink fn):sink(std::move(fn)) {}
    DropSink sink;
    std::atomic<bool> active{true};
};
namespace { std::weak_ptr<DropCapture> current; }
bool test_drop_install_failure=false;
DropToken start_drop_capture(DropSink sink) noexcept {
    if (test_drop_install_failure || !current.expired()) return {};
    try {auto token=std::make_shared<DropCapture>(std::move(sink));current=token;return token;}
    catch (...) {return {};}
}
void stop_drop_capture(DropToken const& token) noexcept {
    if (!token) return;
    token->active=false;
    if (current.lock()==token) current.reset();
}
void test_drop_observation(AnalyticsEvent value) {
    if (auto state=current.lock();state && state->active) state->sink(std::move(value));
}
}
