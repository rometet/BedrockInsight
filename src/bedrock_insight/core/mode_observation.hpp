#pragma once
#include <cstdint>
#include <optional>
#include <limits>

namespace bedrock_insight {
struct ModeObservation {bool before, after;};
class PendingMode final {
public:
    static constexpr std::int64_t kDelayUs=250000, kTtlUs=2000000;
    void note(bool before, bool wanted, std::int64_t now) noexcept {
        if(now<0){clear();return;}
        if(!active || now<first || now-first>kTtlUs){initial=before;first=now;active=true;}
        desired=wanted; // coalesce input, do not count packets or extend TTL
    }
    bool ready(std::int64_t now) noexcept {
        if(active && (now<first || now-first>kTtlUs))clear();
        return active && now-first>=kDelayUs;
    }
    std::optional<ModeObservation> observe(bool actual, std::int64_t now) noexcept {
        if(!ready(now))return {};
        active=false;
        if(initial==desired || actual!=desired)return {};
        return ModeObservation{initial,actual};
    }
    void clear() noexcept {active=false;}
private:
    bool active=false, initial=false, desired=false;
    std::int64_t first=0;
};
inline std::optional<std::int64_t> jst_observation_day(std::int64_t ms) noexcept {
    constexpr std::int64_t offset=32400000;
    if(ms<0 || ms>std::numeric_limits<std::int64_t>::max()-offset)return {};
    return (ms+offset)/86400000;
}
} // namespace bedrock_insight
