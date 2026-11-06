#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class TradeCapture;
using TradeToken = std::shared_ptr<TradeCapture>;
using TradeSink = std::function<void(AnalyticsEvent)>;
// Hooks ServerPlayer::$openTrading (MCAPI 26.60). Emits talked_to_villager only
// when the opened trading target resolves to a villager/wandering trader.
[[nodiscard]] TradeToken start_trade_capture(TradeSink sink) noexcept;
void stop_trade_capture(TradeToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
