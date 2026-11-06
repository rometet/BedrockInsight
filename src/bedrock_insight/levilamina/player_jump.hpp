#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class JumpCapture;
using JumpToken=std::shared_ptr<JumpCapture>;
using JumpSink=std::function<void(AnalyticsEvent)>;
[[nodiscard]] JumpToken start_jump_capture(JumpSink) noexcept;
void stop_jump_capture(JumpToken const&) noexcept;
}
