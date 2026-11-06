#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class DeathCapture;
using DeathToken=std::shared_ptr<DeathCapture>;
using DeathSink=std::function<void(AnalyticsEvent)>;
// Same pinned/pass-through lifecycle contract as player_actions.hpp.
[[nodiscard]] DeathToken start_death_capture(DeathSink sink) noexcept;
void stop_death_capture(DeathToken const& token) noexcept;
}
