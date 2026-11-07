#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>
namespace bedrock_insight::levilamina {
class ResistanceCapture;
using ResistanceToken=std::shared_ptr<ResistanceCapture>;
using ResistanceSink=std::function<void(AnalyticsEvent)>;
ResistanceToken start_resistance_capture(ResistanceSink) noexcept;
void stop_resistance_capture(ResistanceToken const&) noexcept;
}
