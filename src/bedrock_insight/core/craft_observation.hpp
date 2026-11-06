#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace bedrock_insight {

// A completed craft outcome as reported by a successful craft request.
struct CraftedItem final {
    std::string type;
    std::int64_t quantity = 0;
};

// Only a successful request with a valid item type and a positive, bounded
// quantity becomes a record. A failed request, a null/empty result, or an
// implausible count is rejected instead of being counted as an attempt.
[[nodiscard]] inline std::optional<CraftedItem> crafted_item(
    bool was_success,
    std::string type,
    std::int64_t unit_count,
    std::int64_t multiplier
) {
    if (!was_success) return {};
    if (type.empty() || type.size() > 128) return {};
    if (type.find(':') == std::string::npos) return {};
    if (unit_count < 1 || unit_count > 64) return {};
    if (multiplier < 1 || multiplier > 64) return {};
    auto quantity = unit_count * multiplier;
    if (quantity < 1 || quantity > 4096) return {};
    return CraftedItem{std::move(type), quantity};
}

} // namespace bedrock_insight
