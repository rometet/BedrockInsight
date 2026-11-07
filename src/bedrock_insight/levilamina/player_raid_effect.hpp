#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>
namespace bedrock_insight::levilamina {
class RaidEffectCapture;
using RaidEffectToken=std::shared_ptr<RaidEffectCapture>;
using RaidEffectSink=std::function<void(AnalyticsEvent)>;
RaidEffectToken start_raid_effect_capture(RaidEffectSink) noexcept;
void stop_raid_effect_capture(RaidEffectToken const&) noexcept;
}
