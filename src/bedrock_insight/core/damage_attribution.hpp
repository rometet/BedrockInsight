#pragma once
#include <cstdint>
#include <string_view>

namespace bedrock_insight {
// These are copied entry facts, never engine pointers or guessed ABI data.
struct DamageAttributionFacts final {
    int cause=-1;
    bool entity=false,child=false,block=false;
    std::int64_t source_id=0,damaging_id=0;
    bool resolved=false,same_level=false,self=false;
};
inline std::string_view resolved_damage_path(DamageAttributionFacts const& f) noexcept {
    if(!f.entity || f.block || !f.resolved || !f.same_level || f.self ||
       f.source_id==0 || f.damaging_id==0)return {};
    if(f.cause==2 && !f.child && f.source_id==f.damaging_id)return "direct";
    if(f.cause==3 && f.child && f.source_id!=f.damaging_id)return "child_projectile";
    return {}; // No environment, thorns, explosion, command or guessed owner.
}
} // namespace bedrock_insight
