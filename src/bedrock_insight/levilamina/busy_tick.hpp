#pragma once

#include "bedrock_insight/core/busy_tick.hpp"
#include <functional>
#include <memory>

class Level;

namespace bedrock_insight::levilamina {
class BusyTickState;
using BusyTickToken=std::shared_ptr<BusyTickState>;
// Level is valid for this synchronous game-thread callback only; copy primitives
// to the persistence queue. bool=false invalidates provenance, never a sample.
using BusyTickSink=std::function<void(Level&, BusyTickWindow const&, bool)>;

// Explicit error-checked manual registration. Successful hooks stay pass-through
// after disable: the caller MUST pin this DLL before installation. No detach-time
// unhook/destructor work while engine trampolines may still reference the code.
[[nodiscard]] BusyTickToken start_busy_ticks(BusyTickSink sink) noexcept;
void stop_busy_ticks(BusyTickToken const& token) noexcept;
} // namespace bedrock_insight::levilamina
