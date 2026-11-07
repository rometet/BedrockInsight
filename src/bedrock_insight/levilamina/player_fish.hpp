#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>
namespace bedrock_insight::levilamina {
class FishCapture;
using FishToken=std::shared_ptr<FishCapture>;
using FishSink=std::function<void(AnalyticsEvent)>;
FishToken start_fish_capture(FishSink) noexcept;
void stop_fish_capture(FishToken const&) noexcept;
} // namespace bedrock_insight::levilamina
