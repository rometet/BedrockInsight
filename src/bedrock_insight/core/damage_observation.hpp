#pragma once

#include <cmath>
#include <cstdint>

namespace bedrock_insight {

// Converts a float damage value into vanilla tenths (1 heart = 20 tenths) and
// rejects NaN/Inf/absurd values instead of fabricating a statistic.
[[nodiscard]] inline std::int64_t damage_tenths(float damage) noexcept {
    if (!std::isfinite(damage) || damage < 0.0f) return 0;
    double tenths = std::round(static_cast<double>(damage) * 10.0);
    if (tenths < 0.0 || tenths > 1000000.0) return 0;
    return static_cast<std::int64_t>(tenths);
}

// Vanilla damage cause codes are a small enum (-1 None .. 34). Out-of-range
// values are clamped to -1 so a corrupt source cannot poison the key space.
[[nodiscard]] inline int normalized_damage_cause(int code) noexcept {
    return code < -1 || code > 34 ? -1 : code;
}

} // namespace bedrock_insight
