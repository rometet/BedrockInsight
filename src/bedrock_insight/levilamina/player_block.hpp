#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class BlockCapture;
using BlockToken=std::shared_ptr<BlockCapture>;
using BlockSink=std::function<void(AnalyticsEvent)>;
[[nodiscard]] BlockToken start_block_capture(BlockSink) noexcept;
void stop_block_capture(BlockToken const&) noexcept;
}
