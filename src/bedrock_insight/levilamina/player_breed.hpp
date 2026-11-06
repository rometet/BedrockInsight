#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class BreedCapture;
using BreedToken = std::shared_ptr<BreedCapture>;
using BreedSink = std::function<void(AnalyticsEvent)>;
// Hooks BreedableComponent::mate (MCAPI 26.60). Emits animals_bred only when
// the call actually produced offspring AND the stored love cause resolves to a
// tracked player. A feeding attempt without offspring is never counted.
[[nodiscard]] BreedToken start_breed_capture(BreedSink sink) noexcept;
void stop_breed_capture(BreedToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
