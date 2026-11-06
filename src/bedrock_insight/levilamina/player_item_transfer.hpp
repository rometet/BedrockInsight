#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class ItemTransferCapture;
using ItemTransferToken = std::shared_ptr<ItemTransferCapture>;
using ItemTransferSink = std::function<void(AnalyticsEvent)>;

// LK-Stats v0.13.0 src/mod/Hook/ItemStackRequestActionHandler.cpp and
// src/mod/Hook/LevelContainerModel.cpp:
//  - ItemStackRequestActionHandler::_handleTransfer of a CreatedOutputContainer
//    stack is the committed craft / trade / enchant outcome.
//  - LevelContainerModel::$_onItemChanged is the furnace (or smoker) product
//    extraction.
// Both producers run only on the engine's own post-outcome path.
[[nodiscard]] ItemTransferToken start_item_transfer_capture(ItemTransferSink sink) noexcept;
void stop_item_transfer_capture(ItemTransferToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
