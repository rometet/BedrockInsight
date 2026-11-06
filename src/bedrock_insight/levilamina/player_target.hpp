#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class TargetCapture;
using TargetToken = std::shared_ptr<TargetCapture>;
using TargetSink = std::function<void(AnalyticsEvent)>;
// Hooks TargetBlock::$onProjectileHit (MCAPI 26.60). Counts target_hit only when
// the projectile's owner resolves to an online player. A projectile without a
// player owner is never attributed.
[[nodiscard]] TargetToken start_target_capture(TargetSink sink) noexcept;
void stop_target_capture(TargetToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
