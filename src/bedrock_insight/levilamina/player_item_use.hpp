#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class ItemUseCapture;
using ItemUseToken = std::shared_ptr<ItemUseCapture>;
using ItemUseSink = std::function<void(AnalyticsEvent)>;

// LK-Stats v0.13.0 src/mod/Hook/Player/PlayerUseItem.cpp ->
// PlayerItemStatsHandler::onPlayerUsedItem. Player::$useItem is the engine's own
// post-decision use report; a value is queued only after it returns, and only
// for the consuming methods upstream enumerates.
[[nodiscard]] ItemUseToken start_item_use_capture(ItemUseSink sink) noexcept;
void stop_item_use_capture(ItemUseToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
