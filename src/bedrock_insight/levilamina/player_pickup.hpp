#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class PickupCapture;
using PickupToken=std::shared_ptr<PickupCapture>;
using PickupSink=std::function<void(AnalyticsEvent)>;
// DLL pinned by Runtime; logical disable leaves an installed pass-through hook.
[[nodiscard]] PickupToken start_pickup_capture(PickupSink) noexcept;
void stop_pickup_capture(PickupToken const&) noexcept;
}
