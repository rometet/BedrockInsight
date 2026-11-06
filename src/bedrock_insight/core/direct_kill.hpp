#pragma once
#include <cstdint>
#include "bedrock_insight/core/damage_attribution.hpp"

namespace bedrock_insight {
struct DirectAttackFacts {
    int cause=-1;
    bool entity=false,child=false,block=false;
    std::int64_t source_id=0,damaging_id=0;
    bool resolved_player=false,self=false;
};
inline bool credible_direct_attack(DirectAttackFacts const& f) noexcept {
    // Same narrow direct/child paths as killed_by. The adapter separately
    // verifies same-Level resolution to a real Player before calling this.
    return !resolved_damage_path({f.cause,f.entity,f.child,f.block,f.source_id,
        f.damaging_id,f.resolved_player,true,f.self}).empty();
}
inline bool direct_death_transition(bool before,bool after,bool same,bool nested,bool active) noexcept {
    return !before && after && same && !nested && active;
}
} // namespace bedrock_insight
