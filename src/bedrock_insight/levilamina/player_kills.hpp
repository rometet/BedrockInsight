#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class KillCapture;
using KillToken=std::shared_ptr<KillCapture>;
using KillSink=std::function<void(AnalyticsEvent)>;
[[nodiscard]] KillToken start_kill_capture(KillSink) noexcept;
void stop_kill_capture(KillToken const&) noexcept;
}
