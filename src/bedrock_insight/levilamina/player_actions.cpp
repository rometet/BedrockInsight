#include "bedrock_insight/levilamina/player_actions.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class DropCapture final {
public:
    explicit DropCapture(DropSink fn):sink(std::move(fn)) {}
    DropSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex, install_mutex;
DropToken current;
bool installed=false, install_failed=false;
thread_local bool inside_drop=false;
DropToken current_capture() noexcept {
    try { std::lock_guard lock(current_mutex);return current; } catch (...) {return {};}
}
struct DropScope final {
    DropScope() { inside_drop=true; }
    ~DropScope() { inside_drop=false; }
};

LL_TYPE_INSTANCE_HOOK(DropResultHook, ll::memory::HookPriority::Lowest, Player,
    &Player::$drop, bool, ItemStack const& item, bool randomly) {
    auto state=current_capture();
    if (!state || !state->active.load(std::memory_order_acquire) || inside_drop)
        return origin(item,randomly);
    DropScope scope;
    return observe_drop_result(randomly,[&]() -> std::optional<AnalyticsEvent> {
        if (!state->active.load(std::memory_order_acquire)) return {};
        auto const& uuid=getUuid();
        if ((uuid.a==0 && uuid.b==0) || item.mCount==0) return {};
        auto type=item.getTypeName(); // exact pinned LL exported accessor, no NBT
        if (!valid_item_type(type)) return {};
        AnalyticsEvent value;
        value.kind=EventKind::ItemDropResult;
        value.actor.kind=ActorKind::Player;
        value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        value.actor.display_name=getRealName();
        if (value.actor.display_name.size()>256) value.actor.display_name.resize(256);
        value.occurred_at_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        value.primary_type=std::move(type);
        value.amount=item.mCount; // pinned public uchar field, copied BEFORE origin
        value.cause="ll26.60.player_drop_returned_true.v1";
        value.details_json=R"({"hook":"Player::$drop","phase":"return_true","randomly":false,"quantity":"entry_stack_count","inventory_delta_verified":false})";
        return value;
    },[&] { return origin(item,randomly); },[&](AnalyticsEvent value) {
        if (state->active.load(std::memory_order_acquire)) state->sink(std::move(value));
    });
}
}
DropToken start_drop_capture(DropSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if (install_failed || current_capture()) return {};
        if (!installed) {
            if (DropResultHook::hook()!=0) {install_failed=true;return {};}
            installed=true;
        }
        auto state=std::make_shared<DropCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=state;}
        return state;
    } catch (...) {return {};}
}
void stop_drop_capture(DropToken const& token) noexcept {
    if (!token) return;
    token->active.store(false,std::memory_order_release);
    try {std::lock_guard lock(current_mutex);if(current==token)current.reset();} catch (...) {}
}
} // namespace bedrock_insight::levilamina
