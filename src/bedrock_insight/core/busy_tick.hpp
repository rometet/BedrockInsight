#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace bedrock_insight {

// Processing is elapsed steady time inside the Level::$tick detour chain,
// never the inter-frame sleep or mTickDeltaTime. Actual innermost ticks are
// counted separately so warp and freeze do not become false 20 TPS / 50 MSPT.
struct BusyTickWindow final {
    std::int64_t ticks = 0;
    std::int64_t wall_ns = 0;
    std::int64_t busy_ns = 0;
    std::int64_t max_inner_ns = 0;
    std::int64_t frames = 0;
    [[nodiscard]] double tps() const noexcept {
        return wall_ns > 0 ? static_cast<double>(ticks) * 1e9 / static_cast<double>(wall_ns) : 0.0;
    }
    [[nodiscard]] std::optional<double> mspt() const noexcept {
        if (ticks == 0) return {};
        return static_cast<double>(busy_ns) / static_cast<double>(ticks) / 1e6;
    }
};

// Constant-memory disjoint windows, no allocation, sorting, DB or game objects.
// End-to-end wall deltas align the SAME completed frames with their busy time.
class BusyTickSampler final {
public:
    static constexpr std::int64_t kWindowNs = 5'000'000'000;
    static constexpr std::int64_t kMaxWindowNs = 30'000'000'000;
    static constexpr std::int64_t kMaxTicks = 1'000'000;

    [[nodiscard]] std::optional<BusyTickWindow> observe(void const* level,
        std::int64_t start_ns, std::int64_t end_ns, std::int64_t ticks,
        std::int64_t max_inner_ns, bool completed = true) noexcept {
        if (!valid_) return {};
        if (!level || !completed || start_ns < 0 || end_ns < start_ns || ticks < 0 || ticks > kMaxTicks
            || max_inner_ns < 0 || max_inner_ns > end_ns-start_ns || (ticks == 0 && max_inner_ns != 0)) {
            valid_=false; return {};
        }
        if (!level_) { level_=level; last_end_=end_ns; return {}; }
        if (level_ != level || start_ns < last_end_ || end_ns <= last_end_) {
            valid_=false; return {};
        }
        auto const wall=end_ns-last_end_;
        auto const busy=end_ns-start_ns;
        last_end_=end_ns;
        if (wall > kMaxWindowNs || window_.wall_ns > kMaxWindowNs-wall) {
            window_={}; return {}; // pause: require a fresh 5s window
        }
        if (window_.ticks > kMaxTicks-ticks || window_.frames >= kMaxTicks) {
            valid_=false; return {};
        }
        window_.wall_ns+=wall; window_.busy_ns+=busy; window_.ticks+=ticks;
        window_.max_inner_ns=std::max(window_.max_inner_ns,max_inner_ns); ++window_.frames;
        if (window_.wall_ns < kWindowNs) return {};
        auto result=window_; window_={}; return result;
    }
    [[nodiscard]] bool valid() const noexcept { return valid_; }

private:
    void const* level_=nullptr; // identity only, not dereferenced
    std::int64_t last_end_=0;
    BusyTickWindow window_;
    bool valid_=true;
};

} // namespace bedrock_insight
