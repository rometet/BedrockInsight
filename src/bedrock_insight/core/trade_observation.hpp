#pragma once

namespace bedrock_insight {

// ContainerEnumName ordinals used to identify a committed villager trade. They
// are fixed by the pinned 26.60 header enum; the numbers are asserted in tests
// against the header order.
inline constexpr int kCombinedHotbarAndInventoryContainer = 12;
inline constexpr int kHotbarContainer = 28;
inline constexpr int kInventoryContainer = 29;
inline constexpr int kTradeResultPreviewContainer = 33;
inline constexpr int kTrade2ResultPreviewContainer = 49;
inline constexpr int kCursorContainer = 59;

[[nodiscard]] inline bool trade_result_preview(int container) noexcept {
    return container == kTradeResultPreviewContainer || container == kTrade2ResultPreviewContainer;
}

[[nodiscard]] inline bool player_carried_container(int container) noexcept {
    return container == kCombinedHotbarAndInventoryContainer || container == kHotbarContainer
        || container == kInventoryContainer || container == kCursorContainer;
}

// A single committed trade is the successful transfer of the trade result
// preview stack INTO the player's own carried inventory/cursor. Ingredient
// pushes and failures are deliberately excluded so one trade counts once.
[[nodiscard]] inline bool trade_result_to_player(int src_container, int dst_container) noexcept {
    return trade_result_preview(src_container) && player_carried_container(dst_container);
}

} // namespace bedrock_insight
