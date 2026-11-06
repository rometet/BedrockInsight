#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace bedrock_insight {

// One-to-one reproduction of LK-Stats v0.13.0
// `CustomInteractBlockMap` (src/mod/Stats/StatsCustom.h). Only these block
// types produce a CUSTOM interaction, and only from the engine's own
// `Block::use` call after it returned true. Upstream deliberately does NOT list
// lit_*/chipped_anvil/soul_campfire variants; that omission is preserved rather
// than silently widened into a different statistic.
// The returned value is the bare CUSTOM key stored in events.primary_type.
[[nodiscard]] inline std::string_view block_use_key(std::string_view type) noexcept {
    // interactive work blocks / campfire
    if (type == "minecraft:anvil") return "interact_with_anvil";
    if (type == "minecraft:beacon") return "interact_with_beacon";
    if (type == "minecraft:blast_furnace") return "interact_with_blast_furnace";
    if (type == "minecraft:brewing_stand") return "interact_with_brewingstand";
    if (type == "minecraft:campfire") return "interact_with_campfire";
    if (type == "minecraft:cartography_table") return "interact_with_cartography_table";
    if (type == "minecraft:crafting_table") return "interact_with_crafting_table";
    if (type == "minecraft:furnace") return "interact_with_furnace";
    if (type == "minecraft:grindstone") return "interact_with_grindstone";
    if (type == "minecraft:lectern") return "interact_with_lectern";
    if (type == "minecraft:loom") return "interact_with_loom";
    if (type == "minecraft:smithing_table") return "interact_with_smithing_table";
    if (type == "minecraft:smoker") return "interact_with_smoker";
    if (type == "minecraft:stonecutter_block") return "interact_with_stonecutter";
    // inspect
    if (type == "minecraft:dispenser") return "inspect_dispenser";
    if (type == "minecraft:dropper") return "inspect_dropper";
    if (type == "minecraft:hopper") return "inspect_hopper";
    // open containers
    if (type == "minecraft:barrel") return "open_barrel";
    if (type == "minecraft:chest") return "open_chest";
    if (type == "minecraft:ender_chest") return "open_enderchest";
    // open shulker boxes (upstream enumerates every colour)
    if (type == "minecraft:shulker_box") return "open_shulker_box";
    if (type == "minecraft:undyed_shulker_box") return "open_shulker_box";
    if (type == "minecraft:white_shulker_box") return "open_shulker_box";
    if (type == "minecraft:orange_shulker_box") return "open_shulker_box";
    if (type == "minecraft:magenta_shulker_box") return "open_shulker_box";
    if (type == "minecraft:light_blue_shulker_box") return "open_shulker_box";
    if (type == "minecraft:yellow_shulker_box") return "open_shulker_box";
    if (type == "minecraft:lime_shulker_box") return "open_shulker_box";
    if (type == "minecraft:pink_shulker_box") return "open_shulker_box";
    if (type == "minecraft:gray_shulker_box") return "open_shulker_box";
    if (type == "minecraft:light_gray_shulker_box") return "open_shulker_box";
    if (type == "minecraft:cyan_shulker_box") return "open_shulker_box";
    if (type == "minecraft:purple_shulker_box") return "open_shulker_box";
    if (type == "minecraft:blue_shulker_box") return "open_shulker_box";
    if (type == "minecraft:brown_shulker_box") return "open_shulker_box";
    if (type == "minecraft:green_shulker_box") return "open_shulker_box";
    if (type == "minecraft:red_shulker_box") return "open_shulker_box";
    if (type == "minecraft:black_shulker_box") return "open_shulker_box";
    // remaining mapped interactions
    if (type == "minecraft:bell") return "bell_ring";
    if (type == "minecraft:trapped_chest") return "trigger_trapped_chest";
    // Tuning is the Block::use path; playing is the NoteBlock attack path.
    if (type == "minecraft:noteblock") return "tune_noteblock";
    return {};
}

// CauldronBlock::_useInventory classification (upstream BlockEventHandle.cpp):
// a plain bucket is a use, a filled water bucket is a fill.
[[nodiscard]] inline std::string_view cauldron_inventory_key(std::string_view item_type) noexcept {
    if (item_type == "minecraft:bucket") return "use_cauldron";
    if (item_type == "minecraft:water_bucket") return "fill_cauldron";
    return {};
}

// CauldronBlock::_sendCauldronUsedEventToClient with
// POIBlockInteractionType::ClearItem. Upstream classifies the legacy numeric
// item id, so the ranges are preserved verbatim instead of being re-guessed.
[[nodiscard]] inline std::string_view cauldron_clear_key(std::int32_t item_id) noexcept {
    if ((item_id >= -627 && item_id <= -613) || item_id == 218) return "clean_shulker_box";
    if (item_id >= 360 && item_id <= 363) return "clean_armor";
    if (item_id == 600) return "clean_banner";
    return {};
}

// FlowerPotBlock::use: something was planted only when the interaction
// reported success and the pot went from empty to holding a plant.
[[nodiscard]] inline bool flower_pot_planted(bool successful, bool plant_before, bool plant_after) noexcept {
    return successful && !plant_before && plant_after;
}

// The keys the Block::use producer can emit.
inline constexpr std::array<std::string_view, 24> kBlockUseKeys{{
    "interact_with_anvil", "interact_with_beacon", "interact_with_blast_furnace",
    "interact_with_brewingstand", "interact_with_campfire", "interact_with_cartography_table",
    "interact_with_crafting_table", "interact_with_furnace", "interact_with_grindstone",
    "interact_with_lectern", "interact_with_loom", "interact_with_smithing_table",
    "interact_with_smoker", "interact_with_stonecutter", "inspect_dispenser", "inspect_dropper",
    "inspect_hopper", "open_barrel", "open_chest", "open_enderchest", "open_shulker_box", "bell_ring",
    "trigger_trapped_chest", "tune_noteblock",
}};

// Every block-interaction CUSTOM key this adapter can emit, without the
// minecraft: prefix: the 24 Block::use keys plus the dedicated
// cake/cauldron/flower-pot/noteblock producers. Single source of truth for the
// validity predicate and the fixed SQL key list in the reader, so the two
// cannot drift silently.
inline constexpr std::array<std::string_view, 32> kInteractionKeys{{
    "interact_with_anvil", "interact_with_beacon", "interact_with_blast_furnace",
    "interact_with_brewingstand", "interact_with_campfire", "interact_with_cartography_table",
    "interact_with_crafting_table", "interact_with_furnace", "interact_with_grindstone",
    "interact_with_lectern", "interact_with_loom", "interact_with_smithing_table",
    "interact_with_smoker", "interact_with_stonecutter", "inspect_dispenser", "inspect_dropper",
    "inspect_hopper", "open_barrel", "open_chest", "open_enderchest", "open_shulker_box", "bell_ring",
    "trigger_trapped_chest", "tune_noteblock", "eat_cake_slice", "use_cauldron", "fill_cauldron",
    "clean_shulker_box", "clean_armor", "clean_banner", "pot_flower", "play_noteblock",
}};

// True when a key is one of the interaction CUSTOM keys this adapter records.
[[nodiscard]] inline bool interaction_key_valid(std::string_view key) noexcept {
    for (auto candidate : kInteractionKeys) {
        if (candidate == key) return true;
    }
    return false;
}

// The single event cause that produces each interaction key. Every producer is
// post-origin, so a stored row with the matching cause is already a success.
[[nodiscard]] inline std::string_view interaction_key_cause(std::string_view key) noexcept {
    if (key == "eat_cake_slice") return "ll26.60.cake_remove_slice.v1";
    if (key == "use_cauldron" || key == "fill_cauldron") return "ll26.60.cauldron_use_inventory.v1";
    if (key == "clean_shulker_box" || key == "clean_armor" || key == "clean_banner") {
        return "ll26.60.cauldron_clear_item.v1";
    }
    if (key == "pot_flower") return "ll26.60.flower_pot_place.v1";
    if (key == "play_noteblock") return "ll26.60.noteblock_attack.v1";
    return interaction_key_valid(key) ? "ll26.60.block_use_success.v1" : std::string_view{};
}

} // namespace bedrock_insight
