#include "bedrock_insight/levilamina/player_trade.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/Level.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

namespace bedrock_insight::levilamina {
class TradeCapture final {
public:
    explicit TradeCapture(TradeSink fn) : sink(std::move(fn)) {}
    TradeSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex, install_mutex;
TradeToken current;
bool installed_open = false, install_failed = false;
thread_local bool inside_trade = false, nested_trade = false;

TradeToken current_capture() noexcept {
    try { std::lock_guard lock(current_mutex); return current; } catch (...) { return {}; }
}
struct TradeScope final {
    TradeScope() { inside_trade = true; nested_trade = false; }
    ~TradeScope() { inside_trade = false; nested_trade = false; }
};
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
bool villager_trade_target(std::string_view type) noexcept {
    return type == "minecraft:villager" || type == "minecraft:villager_v2"
        || type == "minecraft:wandering_trader";
}

// ServerPlayer::$openTrading is MCAPI in the pinned 26.60 headers. Opening the
// trading screen with a villager is the completed talked_to_villager action; the
// pre-origin right-click alone is not. Only the post-origin target is resolved.
// The completed trade itself is counted by the CreatedOutputContainer transfer
// producer (player_item_transfer.cpp), exactly like the upstream handler.
LL_TYPE_INSTANCE_HOOK(TradeOpenHook, ll::memory::HookPriority::Lowest, ServerPlayer,
    &ServerPlayer::$openTrading, void, ::ActorUniqueID const& targetId, bool useNewScreen) {
    if (inside_trade) { nested_trade = true; origin(targetId, useNewScreen); return; }
    auto state = current_capture();
    if (!state || !state->active.load(std::memory_order_acquire)) { origin(targetId, useNewScreen); return; }
    TradeScope scope;
    std::string id, name;
    Level* level = nullptr;
    try {
        auto const& uuid = getUuid();
        if (uuid.a != 0 || uuid.b != 0) id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
        name = getRealName();
        if (name.size() > 256) name.resize(256);
        level = &getLevel();
    } catch (...) {
        id.clear();
    }
    origin(targetId, useNewScreen);
    if (id.empty() || level == nullptr || nested_trade || targetId.rawID == 0
        || !state->active.load(std::memory_order_acquire)) return;
    try {
        auto* actor = level->fetchEntity(targetId, false);
        if (actor == nullptr) return;
        auto const type = actor->getTypeName();
        if (!valid_item_type(type) || !villager_trade_target(type)) return;
        AnalyticsEvent value;
        value.kind = EventKind::PlayerInteractionResult;
        value.actor.kind = ActorKind::Player;
        value.actor.id = std::move(id);
        value.actor.display_name = std::move(name);
        value.primary_type = "minecraft:talked_to_villager";
        value.secondary_type = type;
        value.cause = "ll26.60.open_trading_villager.v1";
        value.amount = 1;
        value.occurred_at_ms = now_ms();
        value.details_json = "{\"hook\":\"ServerPlayer::$openTrading\",\"phase\":\"post_origin_open\","
            "\"target_type\":\"" + type + "\",\"trade_screen_opened\":true,"
            "\"trade_completed_measured\":true}";
        if (state->active.load(std::memory_order_acquire)) state->sink(std::move(value));
    } catch (...) {}
}
} // namespace

TradeToken start_trade_capture(TradeSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if (install_failed || current_capture()) return {};
        if (!installed_open) {
            if (TradeOpenHook::hook() != 0) { install_failed = true; return {}; }
            installed_open = true;
        }
        auto token = std::make_shared<TradeCapture>(std::move(sink));
        { std::lock_guard lock(current_mutex); current = token; }
        return token;
    } catch (...) { return {}; }
}
void stop_trade_capture(TradeToken const& token) noexcept {
    if (!token) return;
    token->active.store(false, std::memory_order_release);
    try { std::lock_guard lock(current_mutex); if (current == token) current.reset(); } catch (...) {}
}
} // namespace bedrock_insight::levilamina
