#include "bedrock_insight/core/craft_observation.hpp"
#include "bedrock_insight/core/damage_observation.hpp"
#include "bedrock_insight/core/interaction_observation.hpp"
#include "bedrock_insight/core/stats_catalog.hpp"
#include "bedrock_insight/core/trade_observation.hpp"
#include "bedrock_insight/core/travel_observation.hpp"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>

using namespace bedrock_insight;

int main() {
    // --- travel: mode priority and distance units -------------------------
    {
        MovementFlags f;
        assert(classify_travel(f) == TravelMode::Walk);
        f.sprinting = true;
        assert(classify_travel(f) == TravelMode::Sprint);
        f.sneaking = true; // sneaking wins over sprinting
        assert(classify_travel(f) == TravelMode::Sneak);
        f.sneaking = false;
        f.flying = true;
        assert(classify_travel(f) == TravelMode::Fly);
        f.gliding = true; // gliding wins over flying
        assert(classify_travel(f) == TravelMode::Glide);
        f.gliding = false;
        f.flying = false;
        f.on_ground = false;
        assert(classify_travel(f) == TravelMode::Fall);
        f.in_water = true;
        assert(classify_travel(f) == TravelMode::Swim);
        f.swimming = true;
        assert(classify_travel(f) == TravelMode::Swim);
        f = {};
        f.riding = true;
        f.vehicle_type = "minecraft:oak_boat";
        assert(classify_travel(f) == TravelMode::Boat);
        f.vehicle_type = "minecraft:chest_boat";
        assert(classify_travel(f) == TravelMode::Boat);
        f.vehicle_type = "minecraft:minecart";
        assert(classify_travel(f) == TravelMode::Minecart);
        f.vehicle_type = "minecraft:pig";
        assert(classify_travel(f) == TravelMode::RidePig);
        f.vehicle_type = "minecraft:strider";
        assert(classify_travel(f) == TravelMode::RideStrider);
        f.vehicle_type = "minecraft:horse";
        assert(classify_travel(f) == TravelMode::RideHorse);
        f.vehicle_type = "minecraft:nautilus";
        assert(classify_travel(f) == TravelMode::RideNautilus);
        f.vehicle_type = "minecraft:happy_ghast";
        assert(classify_travel(f) == TravelMode::RideHappyGhast);
        f.vehicle_type = "not-namespaced";
        assert(classify_travel(f) == TravelMode::RideOther);
        assert(!valid_vehicle_type("not-namespaced"));
        assert(valid_vehicle_type("minecraft:oak_boat"));
        // Climbing is an explicit ascending-on-climbable state.
        f = {};
        f.on_ground = false;
        f.climbing = true;
        assert(classify_travel(f) == TravelMode::Climb);
        // A vehicle or elytra still wins over a climbable block.
        f.riding = true;
        f.vehicle_type = "minecraft:oak_boat";
        assert(classify_travel(f) == TravelMode::Boat);
        f = {};
        f.on_ground = false;
        f.climbing = true;
        f.gliding = true;
        assert(classify_travel(f) == TravelMode::Glide);
    }
    {
        // Only exact vanilla climbable identifiers qualify.
        assert(is_climbable_block_name("minecraft:ladder"));
        assert(is_climbable_block_name("minecraft:vine"));
        assert(is_climbable_block_name("minecraft:scaffolding"));
        assert(is_climbable_block_name("minecraft:twisting_vines"));
        assert(is_climbable_block_name("minecraft:cave_vines_plant"));
        assert(!is_climbable_block_name("minecraft:stone"));
        assert(!is_climbable_block_name("minecraft:ladder_extra"));
        assert(!is_climbable_block_name("ladder"));
        assert(!is_climbable_block_name(""));
    }
    {
        // 3 blocks right -> 300 cm; vertical ignored for horizontal modes.
        assert(travel_distance_cm(TravelMode::Walk, 3.0, 9.0, 0.0) == 300);
        assert(travel_distance_cm(TravelMode::Sprint, 0.0, 0.0, 4.0) == 400);
        // 3-4-5 -> 500 cm.
        assert(travel_distance_cm(TravelMode::Walk, 3.0, 0.0, 4.0) == 500);
        // Fall counts descent only; climb counts ascent only.
        assert(travel_distance_cm(TravelMode::Fall, 1.0, -2.0, 0.0) == 200);
        assert(travel_distance_cm(TravelMode::Fall, 1.0, 2.0, 0.0) == 0);
        assert(travel_distance_cm(TravelMode::Climb, 0.0, 1.5, 0.0) == 150);
        assert(travel_distance_cm(TravelMode::Climb, 0.0, -1.5, 0.0) == 0);
        // Invalid input is rejected, never fabricated.
        assert(travel_distance_cm(TravelMode::Walk, 0.0, 0.0, 0.0) == 0);
        assert(travel_distance_cm(TravelMode::Walk, 1e9, 0.0, 0.0) == 0);
        assert(travel_distance_cm(TravelMode::Walk, std::numeric_limits<double>::infinity(), 0.0, 0.0) == 0);
        // Normal per-tick movement is continuous; teleport/pearl jumps reset.
        assert(travel_step_is_continuous(1.5, 0.0, -1.5));
        assert(travel_step_is_continuous(0.0, -60.0, 0.0));
        assert(!travel_step_is_continuous(80.0, 0.0, 0.0));
        assert(!travel_step_is_continuous(0.0, 0.0, -96.0));
        assert(!travel_step_is_continuous(std::numeric_limits<double>::infinity(), 0.0, 0.0));
        assert(!travel_step_is_continuous(0.0, std::numeric_limits<double>::infinity(), 0.0));
    }
    // Mode names map to CUSTOM distance keys.
    assert(travel_mode_name(TravelMode::Walk) == "walk");
    assert(travel_mode_name(TravelMode::Sneak) == "crouch");
    assert(travel_mode_name(TravelMode::Glide) == "aviate");
    assert(travel_mode_name(TravelMode::WalkOnWater) == "walk_on_water");
    assert(travel_mode_name(TravelMode::WalkUnderWater) == "walk_under_water");
    // Water surface/underwater are horizontal distance keys like other modes.
    assert(travel_distance_cm(TravelMode::WalkOnWater, 2.0, 0.0, 0.0) == 200);
    assert(travel_distance_cm(TravelMode::WalkUnderWater, 0.0, 0.0, 3.0) == 300);

    // --- interaction mapping (LK-Stats v0.13.0 CustomInteractBlockMap) -----
    assert(block_use_key("minecraft:chest") == "open_chest");
    assert(block_use_key("minecraft:trapped_chest") == "trigger_trapped_chest");
    assert(block_use_key("minecraft:barrel") == "open_barrel");
    assert(block_use_key("minecraft:ender_chest") == "open_enderchest");
    assert(block_use_key("minecraft:light_blue_shulker_box") == "open_shulker_box");
    assert(block_use_key("minecraft:undyed_shulker_box") == "open_shulker_box");
    assert(block_use_key("minecraft:crafting_table") == "interact_with_crafting_table");
    assert(block_use_key("minecraft:stonecutter_block") == "interact_with_stonecutter");
    assert(block_use_key("minecraft:noteblock") == "tune_noteblock");
    assert(block_use_key("minecraft:bell") == "bell_ring");
    // Upstream enumerates these exact ids only: the lit variants and the
    // damaged-anvil variants are deliberately NOT mapped, and the dedicated
    // hooks own cake/cauldron/flower pot.
    assert(block_use_key("minecraft:lit_furnace") == "");
    assert(block_use_key("minecraft:chipped_anvil") == "");
    assert(block_use_key("minecraft:flower_pot") == "");
    assert(block_use_key("minecraft:cake") == "");
    assert(block_use_key("minecraft:stone") == "");
    assert(block_use_key("bad") == "");
    // CauldronBlock::_useInventory: bucket is a use, water bucket is a fill.
    assert(cauldron_inventory_key("minecraft:bucket") == "use_cauldron");
    assert(cauldron_inventory_key("minecraft:water_bucket") == "fill_cauldron");
    assert(cauldron_inventory_key("minecraft:glass_bottle") == "");
    // CauldronBlock::_sendCauldronUsedEventToClient with ClearItem.
    assert(cauldron_clear_key(-620) == "clean_shulker_box");
    assert(cauldron_clear_key(218) == "clean_shulker_box");
    assert(cauldron_clear_key(-612) == "");
    assert(cauldron_clear_key(360) == "clean_armor");
    assert(cauldron_clear_key(363) == "clean_armor");
    assert(cauldron_clear_key(364) == "");
    assert(cauldron_clear_key(600) == "clean_banner");
    assert(cauldron_clear_key(599) == "");
    // FlowerPotBlock::use needs success plus an empty -> planted transition.
    assert(flower_pot_planted(true, false, true));
    assert(!flower_pot_planted(false, false, true));
    assert(!flower_pot_planted(true, true, true));
    assert(!flower_pot_planted(true, false, false));
    {
        assert(kBlockUseKeys.size() == 24);
        assert(kInteractionKeys.size() == 32);
        for (auto key : kBlockUseKeys) {
            assert(interaction_key_valid(key));
            assert(is_custom_key("minecraft:" + std::string(key)));
        }
        for (auto key : kInteractionKeys) {
            assert(interaction_key_valid(key));
            // Every emitted interaction key must be one of the 77 CUSTOM names.
            assert(is_custom_key("minecraft:" + std::string(key)));
        }
    }

    // --- trade commit classification --------------------------------------
    {
        // Pinned ContainerEnumName ordinals.
        assert(kCombinedHotbarAndInventoryContainer == 12);
        assert(kHotbarContainer == 28);
        assert(kInventoryContainer == 29);
        assert(kTradeResultPreviewContainer == 33);
        assert(kTrade2ResultPreviewContainer == 49);
        assert(kCursorContainer == 59);
        // Trade result preview -> player inventory/cursor is ONE committed trade.
        assert(trade_result_to_player(33, 12));
        assert(trade_result_to_player(49, 29));
        assert(trade_result_to_player(33, 59));
        // Ingredients, crafting output and player-to-trade pushes never count.
        assert(!trade_result_to_player(31, 33));
        assert(!trade_result_to_player(32, 33));
        assert(!trade_result_to_player(33, 13));
        assert(!trade_result_to_player(12, 33));
        assert(!trade_result_to_player(14, 12));
        assert(!trade_result_to_player(-1, 12));
    }

    // --- damage -----------------------------------------------------------
    assert(damage_tenths(3.0f) == 30);
    assert(damage_tenths(0.5f) == 5);
    assert(damage_tenths(-1.0f) == 0);
    assert(damage_tenths(std::numeric_limits<float>::infinity()) == 0);
    assert(normalized_damage_cause(-1) == -1);
    assert(normalized_damage_cause(4) == 4);
    assert(normalized_damage_cause(999) == -1);

    // --- craft ------------------------------------------------------------
    {
        auto good = crafted_item(true, "minecraft:stick", 4, 3);
        assert(good && good->quantity == 12 && good->type == "minecraft:stick");
        assert(!crafted_item(false, "minecraft:stick", 4, 3));   // failed request
        assert(!crafted_item(true, "", 4, 3));                    // no type
        assert(!crafted_item(true, "stick", 4, 3));               // not namespaced
        assert(!crafted_item(true, "minecraft:stick", 0, 3));     // zero units
        assert(!crafted_item(true, "minecraft:stick", 4, 0));     // zero multiplier
        assert(!crafted_item(true, "minecraft:stick", 64, 65));   // bounded result
        assert(!crafted_item(true, "minecraft:stick", 65, 1));    // unit above stack
    }

    // --- catalog ----------------------------------------------------------
    assert(kCategories.size() == 9);
    assert(kCustomKeys.size() == 77);
    {
        // Every key is unique and namespaced.
        for (std::size_t i = 0; i < kCustomKeys.size(); ++i) {
            assert(kCustomKeys[i].rfind("minecraft:", 0) == 0);
            for (std::size_t j = i + 1; j < kCustomKeys.size(); ++j) assert(kCustomKeys[i] != kCustomKeys[j]);
        }
        assert(is_custom_key("minecraft:walk_one_cm"));
        assert(is_custom_key("minecraft:happy_ghast_one_cm"));
        assert(is_custom_key("minecraft:time_since_death"));
        assert(!is_custom_key("minecraft:not_a_stat"));
        // Documented upstream exceptions stay present rather than invented away.
        assert(is_custom_key("minecraft:time_since_rest"));
    }
    assert(category_from_id("minecraft:mined") == StatsCategory::Mined);
    assert(category_from_id("crafted") == StatsCategory::Crafted);
    assert(category_from_id("minecraft:killed_by") == StatsCategory::KilledBy);
    assert(category_from_id("nonsense") == StatsCategory::Unknown);
    assert(category_info(StatsCategory::Mined).label == "採掘したブロック");
    // Unknown CUSTOM keys fall back to the key itself, never to a blank label.
    assert(custom_key_label("minecraft:unknown_future_key") == "minecraft:unknown_future_key");

    std::cout << "Parity observation PASS: travel modes/distances, interactions, damage, craft gating, 9 categories + 77 keys\n";
}
