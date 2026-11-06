#pragma once
#include <cmath>
#include <cstdint>
#include <optional>

namespace bedrock_insight {
struct JumpState {
    std::int64_t y_micro=0;
    std::int32_t dimension=0;
    bool grounded=false,vehicle=false,dead=false;
};
inline std::optional<std::int64_t> jump_y_micro(double y) noexcept {
    if(!std::isfinite(y) || y < -16.0 || y > 16.0)return {};
    return static_cast<std::int64_t>(std::llround(y*1000000.0));
}
inline bool observed_upward_impulse(JumpState const& before,JumpState const& after,
    bool same_actor,bool nested) noexcept {
    return same_actor && !nested && before.grounded && !before.vehicle && !after.vehicle &&
        !before.dead && !after.dead && before.dimension==after.dimension &&
        before.y_micro>=-16000000 && before.y_micro<=16000000 &&
        after.y_micro>0 && after.y_micro<=16000000 && after.y_micro>before.y_micro;
}
} // namespace bedrock_insight
