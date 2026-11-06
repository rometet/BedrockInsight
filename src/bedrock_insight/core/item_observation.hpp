#pragma once

#include <cstdint>
#include <string_view>

namespace bedrock_insight {

// Pinned 26.60 ItemUseMethod ordinals used by the ported
// PlayerItemStatsHandler::onPlayerUsedItem switch.
inline constexpr int kItemUseEat = 1;
inline constexpr int kItemUseConsume = 3;
inline constexpr int kItemUseThrow = 4;
inline constexpr int kItemUsePlace = 6;
inline constexpr int kItemUseFillBottle = 7;
inline constexpr int kItemUsePourBucket = 9;
inline constexpr int kItemUseInteract = 11;

// Pinned 26.60 SharedTypes::Legacy::ContainerType ordinals.
inline constexpr int kContainerTypeInventory = -1;
inline constexpr int kContainerTypeWorkbench = 1;
inline constexpr int kContainerTypeEnchantment = 3;
inline constexpr int kContainerTypeTrade = 15;

[[nodiscard]] inline std::string_view item_use_method_name(int method) noexcept {
    switch (method) {
    case 0: return "equip_armor";
    case kItemUseEat: return "eat";
    case 2: return "attack";
    case kItemUseConsume: return "consume";
    case kItemUseThrow: return "throw";
    case 5: return "shoot";
    case kItemUsePlace: return "place";
    case kItemUseFillBottle: return "fill_bottle";
    case 8: return "fill_bucket";
    case kItemUsePourBucket: return "pour_bucket";
    case 10: return "use_tool";
    case kItemUseInteract: return "interact";
    case 12: return "retrieved";
    case 13: return "dyed";
    case 14: return "traded";
    case 15: return "brushing_completed";
    case 16: return "opened_vault";
    default: return "unknown";
    }
}

// LK-Stats v0.13.0 onPlayerUsedItem: only a consuming use counts, and only for
// the methods it enumerates. Bone meal is the sole Interact item that counts.
[[nodiscard]] inline bool use_counts_as_used(int item_use_method, std::string_view item_type) noexcept {
    switch (item_use_method) {
    case kItemUseEat:
    case kItemUseConsume:
    case kItemUseThrow:
    case kItemUseFillBottle:
    case kItemUsePourBucket:
    case kItemUsePlace:
        return true;
    case kItemUseInteract:
        return item_type == "minecraft:bone_meal";
    default:
        return false;
    }
}

// Placing a music disc is the only Place method that also plays a record.
[[nodiscard]] inline bool use_place_plays_record(int item_use_method) noexcept {
    return item_use_method == kItemUsePlace;
}

// onPlayerItemHurtAndBroken: durability damage on a non-armor damageable item
// owned by a player is the upstream `used` counter for tools and weapons.
[[nodiscard]] inline bool item_hurt_counts_as_used(
    bool owner_is_player,
    bool damageable,
    bool armor,
    int delta_damage) noexcept {
    return owner_is_player && damageable && !armor && delta_damage > 0;
}

// onPlayerChangeContainerWith: extracting the finished product from a furnace
// (or smoker) output slot.
[[nodiscard]] inline bool furnace_output_extract(
    std::string_view block_type,
    int slot,
    int old_count,
    int new_count) noexcept {
    if (block_type != "minecraft:furnace" && block_type != "minecraft:lit_furnace"
        && block_type != "minecraft:smoker") {
        return false;
    }
    return slot == 2 && old_count > 0 && new_count >= 0 && old_count > new_count;
}

// onPlayerCraftedItem: the open screen decides how a created-output transfer is
// classified. Empty means the transfer is not counted.
[[nodiscard]] inline std::string_view created_output_branch(int screen_container_type) noexcept {
    switch (screen_container_type) {
    case kContainerTypeTrade: return "trade";
    case kContainerTypeEnchantment: return "enchantment";
    case kContainerTypeWorkbench: return "workbench";
    case kContainerTypeInventory: return "inventory";
    default: return {};
    }
}

// A created-output transfer only counts with a plausible stack count.
[[nodiscard]] inline bool created_output_amount_valid(int amount, bool type_valid) noexcept {
    return type_valid && amount >= 1 && amount <= 255;
}

} // namespace bedrock_insight
