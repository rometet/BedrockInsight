#pragma once
#include "bedrock_insight/core/travel_observation.hpp"
#include <array>
#include <cstddef>
#include <optional>

namespace bedrock_insight {
inline constexpr std::size_t kTravelModeCount = static_cast<std::size_t>(TravelMode::WalkUnderWater) + 1;
struct TravelFrame final {
    double x = 0, y = 0, z = 0;
    std::int64_t steady_us = 0, utc_ms = 0, vehicle_id = 0;
    std::int32_t dimension = 0;
    bool alive = true, mounted = false;
    TravelMode mode = TravelMode::Walk;
};
struct TravelBucket final {
    double centimeters = 0;
    std::int64_t first_ms = 0, last_ms = 0;
    std::int32_t dimension = 0;
    std::uint32_t samples = 0;
    [[nodiscard]] std::int64_t whole_cm() const noexcept {
        return static_cast<std::int64_t>(std::floor(centimeters));
    }
};

// Independent fixed-memory server-position sampler. No client packets, engine
// pointers, clocks, queue or SQLite dependency. Caller drains BEFORE a boundary.
class TravelWindow final {
public:
    using Buckets = std::array<TravelBucket, kTravelModeCount>;
    static bool valid(TravelFrame const& f) noexcept {
        return std::isfinite(f.x) && std::isfinite(f.y) && std::isfinite(f.z)
            && f.steady_us >= 0 && f.utc_ms >= 0 && (!f.mounted || f.vehicle_id != 0)
            && static_cast<std::size_t>(f.mode) < kTravelModeCount;
    }
    static std::int64_t jst_day(std::int64_t t) noexcept {
        return t / 86400000 + (t % 86400000 + 32400000) / 86400000;
    }
    [[nodiscard]] bool boundary(TravelFrame const& f) const noexcept {
        if (!valid(f)) return true;
        if (!previous_) return false;
        auto const& p = *previous_;
        if (!p.alive || !f.alive || f.dimension != p.dimension || f.mounted != p.mounted
            || f.vehicle_id != p.vehicle_id || f.utc_ms < p.utc_ms || jst_day(f.utc_ms) != jst_day(p.utc_ms)
            || f.steady_us <= p.steady_us || f.steady_us - p.steady_us > 1000000) return true;
        // Conservative per-observation discontinuity rejection, NOT proof that
        // every short teleport is detected. Gaps >1s are lower-bound omissions.
        return std::hypot(std::hypot(f.x - p.x, f.z - p.z), f.y - p.y) > 10.0;
    }
    void reset(TravelFrame const& f) noexcept {
        buckets_ = {};
        previous_ = valid(f) ? std::optional<TravelFrame>(f) : std::nullopt;
        flushed_us_ = f.steady_us;
    }
    void observe(TravelFrame const& f) noexcept {
        if (boundary(f)) { reset(f); return; }
        if (!previous_) { reset(f); return; }
        auto const& p = *previous_;
        auto const cm = 100.0 * travel_distance_blocks(f.mode, f.x - p.x, f.y - p.y, f.z - p.z);
        if (cm > 0 && cm <= 1000.0) {
            auto& b = buckets_[static_cast<std::size_t>(f.mode)];
            if (b.centimeters + cm <= 100000000.0 && b.samples < 1000000) {
                if (!b.samples) { b.first_ms = f.utc_ms; b.dimension = f.dimension; }
                b.last_ms = f.utc_ms; ++b.samples; b.centimeters += cm;
            }
        }
        previous_ = f;
    }
    [[nodiscard]] bool due(std::int64_t steady_us) const noexcept {
        return previous_ && steady_us >= flushed_us_ && steady_us - flushed_us_ >= 5000000;
    }
    [[nodiscard]] bool ascending(double y) const noexcept { return previous_ && y > previous_->y; }
    Buckets drain() noexcept {
        auto copy = buckets_; buckets_ = {};
        if (previous_) flushed_us_ = previous_->steady_us;
        return copy;
    }
private:
    std::optional<TravelFrame> previous_;
    Buckets buckets_{};
    std::int64_t flushed_us_ = 0;
};
} // namespace bedrock_insight
