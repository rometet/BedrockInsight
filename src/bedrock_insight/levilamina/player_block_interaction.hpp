#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class BlockInteractionCapture;
using BlockInteractionToken = std::shared_ptr<BlockInteractionCapture>;
using BlockInteractionSink = std::function<void(AnalyticsEvent)>;

// One-to-one port of LK-Stats v0.13.0 src/mod/Hook/Block/*:
//   Block.cpp          -> Block::use          (CustomInteractBlockMap keys)
//   CakeBlock.cpp      -> CakeBlock::removeCakeSlice
//   CauldronBlock.cpp  -> CauldronBlock::_useInventory / _sendCauldronUsedEventToClient
//   FlowerPotBlock.cpp -> FlowerPotBlock::use
//   NoteBlock.cpp      -> NoteBlock::$attack
// Every producer is post-origin: a value is queued only after the engine's own
// call reported the success condition upstream uses.
[[nodiscard]] BlockInteractionToken start_block_interaction_capture(BlockInteractionSink sink) noexcept;
void stop_block_interaction_capture(BlockInteractionToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
