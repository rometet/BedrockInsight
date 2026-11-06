#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class HurtCapture;
using HurtToken=std::shared_ptr<HurtCapture>;
using HurtSink=std::function<void(AnalyticsEvent)>;
HurtToken start_hurt_capture(HurtSink) noexcept;
void stop_hurt_capture(HurtToken const&) noexcept;
}
