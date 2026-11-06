#pragma once
#include "bedrock_insight/core/drop_observation.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace bedrock_insight {
// A successful break is proven ONLY by the block at the same position changing
// away from the pre-call type after the engine call. The returning bool alone is
// not used as proof; it is recorded for diagnostics.
inline bool block_break_confirmed(std::string_view before_type,
    std::string_view after_type) noexcept {
    if (!valid_item_type(before_type) || !valid_item_type(after_type)) return false;
    if (before_type == "minecraft:air") return false;
    return before_type != after_type;
}

// Placement: the position must have changed AND its new state must not be air.
// Returns the placed namespaced block type, or empty for no proven transition.
inline std::optional<std::string> placed_block_type(std::string_view before_type,
    std::string_view after_type) {
    if (!valid_item_type(before_type) || !valid_item_type(after_type)) return {};
    if (after_type == "minecraft:air") return {};
    if (before_type == after_type) return {};
    return std::string(after_type);
}

// Copies the entry snapshot before origin(), lets origin run exactly once, and
// only then re-reads state. Instrumentation never skips the engine call or
// replaces its bool/exception; a missing/ambiguous snapshot emits nothing.
template<class Snapshot, class Origin, class After, class Emit>
bool observe_block_outcome(Snapshot&& snapshot, Origin&& origin, After&& after, Emit&& emit) {
    decltype(snapshot()) entry;
    try { entry = snapshot(); } catch (...) {}
    bool const accepted = origin();
    if (accepted && entry) {
        try { if (after(*entry)) emit(std::move(*entry)); } catch (...) {}
    }
    return accepted;
}
} // namespace bedrock_insight
