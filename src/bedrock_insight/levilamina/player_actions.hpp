#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>

namespace bedrock_insight::levilamina {
class DropCapture;
using DropToken=std::shared_ptr<DropCapture>;
using DropSink=std::function<void(AnalyticsEvent)>;
// Caller must pin the DLL before installing. Logical disable makes installed
// hooks pass through; no detach-time unhook or references to mutable MC objects.
[[nodiscard]] DropToken start_drop_capture(DropSink sink) noexcept;
void stop_drop_capture(DropToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
