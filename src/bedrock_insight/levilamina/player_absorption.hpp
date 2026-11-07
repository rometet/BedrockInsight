#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>
namespace bedrock_insight::levilamina {
class AbsorptionCapture;
using AbsorptionToken=std::shared_ptr<AbsorptionCapture>;
using AbsorptionSink=std::function<void(AnalyticsEvent)>;
AbsorptionToken start_absorption_capture(AbsorptionSink) noexcept;
void stop_absorption_capture(AbsorptionToken const&) noexcept;
}
