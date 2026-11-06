#pragma once
#include <cstdint>
#include <optional>
#include <utility>

namespace bedrock_insight {
// Net increase in the same bounded main inventory, not entry stack size or a
// promise of durable transfer. Reject ambiguous gains, size changes and underflow.
inline std::optional<std::int64_t> pickup_inventory_gain(int entry, int slots_before,
    int slots_after, std::int64_t before, std::int64_t after, bool same_inventory) noexcept {
    if(!same_inventory || entry<1 || entry>255 || slots_before<1 || slots_before>128 ||
       slots_after!=slots_before || before<0 || after<0 ||
       before>slots_before*255 || after>slots_before*255 || after<=before || after-before>entry)return {};
    return after-before;
}

// Observe only after the engine returns true. Instrumentation never skips the
// engine call, changes its bool/exception, or reads a stale entry reference itself.
template<class Snapshot,class Origin,class After,class Emit>
bool observe_pickup_result(Snapshot&& snapshot,Origin&& origin,After&& after,Emit&& emit) {
    decltype(snapshot()) entry;
    try {entry=snapshot();}catch(...){}
    bool const accepted=origin();
    if(accepted && entry) {
        try {if(after(*entry))emit(std::move(*entry));}catch(...){}
    }
    return accepted;
}
} // namespace bedrock_insight
