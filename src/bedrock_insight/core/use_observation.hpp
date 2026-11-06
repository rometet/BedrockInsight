#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>

namespace bedrock_insight {
// A type-total decrease in the SAME main inventory during a true-returning
// useItem call. This is not proof of consumption, effect completion or causality.
inline std::optional<std::int64_t> use_inventory_loss(int entry,int slots_before,
    int slots_after,std::int64_t before,std::int64_t after,bool same_inventory) noexcept {
    if(!same_inventory || entry<1 || entry>255 || slots_before<1 || slots_before>128 ||
       slots_after!=slots_before || before<0 || after<0 || before>slots_before*255 ||
       after>slots_before*255 || before-after>entry)return {};
    return std::max<std::int64_t>(0,before-after);
}
}
