#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace bedrock_insight {

// Interval between the SAME ServerLevel::_subTick pre-origin callbacks, NOT
// time spent processing a tick. No allocation/I/O and O(1) work per callback.
struct TickIntervalWindow final {
    std::int64_t elapsed_us = 0;
    std::int64_t intervals = 0;
    [[nodiscard]] double effective_tps() const noexcept {
        return elapsed_us > 0 && intervals > 0
            ? std::min(20.0, 1'000'000.0 * static_cast<double>(intervals) / static_cast<double>(elapsed_us)) : 0.0;
    }
};

class TickIntervalSampler final {
public:
    static constexpr std::int64_t kWindowUs = 5'000'000;
    static constexpr std::int64_t kMaxWindowUs = 30'000'000;

    [[nodiscard]] std::optional<TickIntervalWindow> observe(void const* level, std::int64_t steady_us) noexcept {
        if (!supported_ || !level || steady_us < 0) return {};
        if (!level_) { level_ = level; start_us_ = last_us_ = steady_us; return {}; }
        // Never interleave Levels/dimensions or retain a game object to dereference.
        // A second Level invalidates the entire activation rather than doubling TPS.
        if (level_ != level) { supported_ = false; return {}; }
        if (steady_us <= last_us_) return {}; // duplicate timestamp/backward input
        last_us_ = steady_us;
        auto const elapsed = steady_us - start_us_;
        if (elapsed > kMaxWindowUs || ++intervals_ > 10000) {
            start_us_ = steady_us; intervals_ = 0; return {}; // pause: fresh warmup
        }
        if (elapsed < kWindowUs) return {};
        TickIntervalWindow result{elapsed, intervals_};
        start_us_ = steady_us; intervals_ = 0;
        return result;
    }
    [[nodiscard]] bool supported() const noexcept { return supported_; }

private:
    void const* level_ = nullptr; // identity only, never dereferenced
    std::int64_t start_us_ = 0, last_us_ = 0, intervals_ = 0;
    bool supported_ = true;
};

} // namespace bedrock_insight
