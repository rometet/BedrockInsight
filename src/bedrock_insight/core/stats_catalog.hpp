#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace bedrock_insight {

// The nine user-facing LK-Stats v0.13.0 statistic categories. The string form is
// the upstream namespaced identifier, kept for command/API input.
enum class StatsCategory : std::uint8_t {
    Custom,
    Mined,
    Broken,
    Crafted,
    Used,
    PickedUp,
    Dropped,
    Killed,
    KilledBy,
    Unknown,
};

struct CategoryName final {
    StatsCategory category;
    std::string_view id;       // minecraft:mined
    std::string_view short_id; // mined
    std::string_view label;    // 日本語表示
};

inline constexpr std::array<CategoryName, 9> kCategories{{
    {StatsCategory::Custom, "minecraft:custom", "custom", "カスタム統計"},
    {StatsCategory::Mined, "minecraft:mined", "mined", "採掘したブロック"},
    {StatsCategory::Broken, "minecraft:broken", "broken", "破損した道具"},
    {StatsCategory::Crafted, "minecraft:crafted", "crafted", "クラフトしたアイテム"},
    {StatsCategory::Used, "minecraft:used", "used", "使用したアイテム"},
    {StatsCategory::PickedUp, "minecraft:picked_up", "picked_up", "拾ったアイテム"},
    {StatsCategory::Dropped, "minecraft:dropped", "dropped", "捨てたアイテム"},
    {StatsCategory::Killed, "minecraft:killed", "killed", "倒したエンティティ"},
    {StatsCategory::KilledBy, "minecraft:killed_by", "killed_by", "倒された原因"},
}};

[[nodiscard]] inline CategoryName const& category_info(StatsCategory category) noexcept {
    for (auto const& entry : kCategories) {
        if (entry.category == category) return entry;
    }
    return kCategories[0];
}

[[nodiscard]] inline StatsCategory category_from_id(std::string_view id) noexcept {
    for (auto const& entry : kCategories) {
        if (entry.id == id || entry.short_id == id) return entry.category;
    }
    return StatsCategory::Unknown;
}

// The 77 CUSTOM identifiers defined by the pinned LK-Stats v0.13.0 enum.
inline constexpr std::array<std::string_view, 77> kCustomKeys{{
    "minecraft:animals_bred", "minecraft:clean_armor", "minecraft:clean_banner", "minecraft:open_barrel",
    "minecraft:bell_ring", "minecraft:eat_cake_slice", "minecraft:fill_cauldron", "minecraft:open_chest",
    "minecraft:damage_absorbed", "minecraft:damage_blocked_by_shield", "minecraft:damage_dealt",
    "minecraft:damage_dealt_absorbed", "minecraft:damage_dealt_resisted", "minecraft:damage_resisted",
    "minecraft:damage_taken", "minecraft:inspect_dispenser", "minecraft:nautilus_one_cm",
    "minecraft:happy_ghast_one_cm", "minecraft:boat_one_cm", "minecraft:aviate_one_cm", "minecraft:horse_one_cm",
    "minecraft:minecart_one_cm", "minecraft:pig_one_cm", "minecraft:strider_one_cm", "minecraft:climb_one_cm",
    "minecraft:crouch_one_cm", "minecraft:fall_one_cm", "minecraft:fly_one_cm", "minecraft:sprint_one_cm",
    "minecraft:swim_one_cm", "minecraft:walk_one_cm", "minecraft:walk_on_water_one_cm",
    "minecraft:walk_under_water_one_cm", "minecraft:inspect_dropper", "minecraft:open_enderchest",
    "minecraft:fish_caught", "minecraft:leave_game", "minecraft:inspect_hopper", "minecraft:interact_with_anvil",
    "minecraft:interact_with_beacon", "minecraft:interact_with_blast_furnace",
    "minecraft:interact_with_brewingstand", "minecraft:interact_with_campfire",
    "minecraft:interact_with_cartography_table", "minecraft:interact_with_crafting_table",
    "minecraft:interact_with_furnace", "minecraft:interact_with_grindstone", "minecraft:interact_with_lectern",
    "minecraft:interact_with_loom", "minecraft:interact_with_smithing_table", "minecraft:interact_with_smoker",
    "minecraft:interact_with_stonecutter", "minecraft:drop", "minecraft:enchant_item", "minecraft:jump",
    "minecraft:mob_kills", "minecraft:play_record", "minecraft:play_noteblock", "minecraft:tune_noteblock",
    "minecraft:deaths", "minecraft:pot_flower", "minecraft:player_kills", "minecraft:raid_trigger",
    "minecraft:raid_win", "minecraft:clean_shulker_box", "minecraft:open_shulker_box",
    "minecraft:time_since_death", "minecraft:time_since_rest", "minecraft:sneak_time",
    "minecraft:talked_to_villager", "minecraft:target_hit", "minecraft:play_time", "minecraft:total_world_time",
    "minecraft:sleep_in_bed", "minecraft:traded_with_villager", "minecraft:trigger_trapped_chest",
    "minecraft:use_cauldron",
}};

[[nodiscard]] inline bool is_custom_key(std::string_view key) noexcept {
    for (auto candidate : kCustomKeys) {
        if (candidate == key) return true;
    }
    return false;
}

// Japanese label for the CUSTOM keys this build actually records, with a safe
// key fallback for the rest. Missing labels must never hide a real record.
[[nodiscard]] inline std::string_view custom_key_label(std::string_view key) noexcept {
    if (key == "minecraft:walk_one_cm") return "歩行距離";
    if (key == "minecraft:sprint_one_cm") return "スプリント距離";
    if (key == "minecraft:crouch_one_cm") return "スニーク移動距離";
    if (key == "minecraft:swim_one_cm") return "水中游泳距離";
    if (key == "minecraft:fly_one_cm") return "飛行距離";
    if (key == "minecraft:aviate_one_cm") return "エリトラ滑空距離";
    if (key == "minecraft:fall_one_cm") return "落下距離";
    if (key == "minecraft:boat_one_cm") return "ボート移動距離";
    if (key == "minecraft:minecart_one_cm") return "トロッコ移動距離";
    if (key == "minecraft:horse_one_cm") return "騎乗(馬)距離";
    if (key == "minecraft:pig_one_cm") return "騎乗(ブタ)距離";
    if (key == "minecraft:strider_one_cm") return "騎乗(ストライダー)距離";
    if (key == "minecraft:nautilus_one_cm") return "騎乗(ナウティラス)距離";
    if (key == "minecraft:happy_ghast_one_cm") return "騎乗(ハッピーガスト)距離";
    if (key == "minecraft:climb_one_cm") return "登攀距離";
    if (key == "minecraft:walk_on_water_one_cm") return "水面歩行距離";
    if (key == "minecraft:walk_under_water_one_cm") return "水中歩行距離";
    if (key == "minecraft:animals_bred") return "動物の繁殖";
    if (key == "minecraft:damage_absorbed") return "吸収属性の減少観測（丸め）";
    if (key == "minecraft:damage_blocked_by_shield") return "盾で防いだダメージ";
    if (key == "minecraft:damage_dealt_absorbed") return "加害先の吸収属性減少観測（丸め）";
    if (key == "minecraft:damage_dealt_resisted") return "加害先の耐性量（式・丸め）";
    if (key == "minecraft:damage_resisted") return "自身の耐性量（式・丸め）";
    if (key == "minecraft:fish_caught") return "水生Mobのバケツ捕獲観測";
    if (key == "minecraft:leave_game") return "退出回数";
    if (key == "minecraft:drop") return "投棄個数";
    if (key == "minecraft:enchant_item") return "エンチャントした回数";
    if (key == "minecraft:jump") return "ジャンプ通知（旧加速観測を含む）";
    if (key == "minecraft:mob_kills") return "Mob討伐回数";
    if (key == "minecraft:deaths") return "死亡回数";
    if (key == "minecraft:player_kills") return "プレイヤー討伐回数";
    if (key == "minecraft:raid_trigger") return "襲撃前兆効果の付与観測";
    if (key == "minecraft:raid_win") return "村の英雄効果の付与観測";
    if (key == "minecraft:time_since_death") return "最後の死亡後の時間（完結セッション下限・換算tick）";
    if (key == "minecraft:time_since_rest") return "睡眠観測後のPlayer tick（最終保存値）";
    if (key == "minecraft:sneak_time") return "スニーク時間";
    if (key == "minecraft:talked_to_villager") return "村人と話した回数";
    if (key == "minecraft:target_hit") return "的への命中回数";
    if (key == "minecraft:play_time") return "プレイ時間（完結セッション下限・換算tick）";
    if (key == "minecraft:total_world_time") return "ワールド共有tick（最終観測値）";
    if (key == "minecraft:traded_with_villager") return "村人との取引回数";
    if (key == "minecraft:damage_taken") return "自身のHP属性減少観測（0.1HP・丸め）";
    if (key == "minecraft:damage_dealt") return "加害先のHP属性減少観測（0.1HP・丸め）";
    if (key == "minecraft:open_chest") return "チェストを開いた";
    if (key == "minecraft:open_barrel") return "樽を開いた";
    if (key == "minecraft:open_enderchest") return "エンダーチェストを開いた";
    if (key == "minecraft:open_shulker_box") return "シュルカーボックスを開いた";
    if (key == "minecraft:interact_with_crafting_table") return "作業台を使用";
    if (key == "minecraft:interact_with_furnace") return "かまどを使用";
    if (key == "minecraft:interact_with_blast_furnace") return "溶鉱炉を使用";
    if (key == "minecraft:interact_with_smoker") return "燻製器を使用";
    if (key == "minecraft:interact_with_anvil") return "金床を使用";
    if (key == "minecraft:interact_with_beacon") return "ビーコンを使用";
    if (key == "minecraft:interact_with_brewingstand") return "醸造台を使用";
    if (key == "minecraft:interact_with_campfire") return "焚き火を使用";
    if (key == "minecraft:interact_with_cartography_table") return "製図台を使用";
    if (key == "minecraft:interact_with_grindstone") return "砥石を使用";
    if (key == "minecraft:interact_with_lectern") return "書見台を使用";
    if (key == "minecraft:interact_with_loom") return "織機を使用";
    if (key == "minecraft:interact_with_smithing_table") return "鍛冶台を使用";
    if (key == "minecraft:interact_with_stonecutter") return "石切台を使用";
    if (key == "minecraft:inspect_dispenser") return "ディスペンサーを確認";
    if (key == "minecraft:inspect_dropper") return "ドロッパーを確認";
    if (key == "minecraft:inspect_hopper") return "ホッパーを確認";
    if (key == "minecraft:play_noteblock") return "音符ブロックを鳴らした";
    if (key == "minecraft:tune_noteblock") return "音符ブロックを調整した";
    if (key == "minecraft:bell_ring") return "鐘を鳴らした";
    if (key == "minecraft:pot_flower") return "植木鉢に植えた";
    if (key == "minecraft:eat_cake_slice") return "ケーキを食べた";
    if (key == "minecraft:use_cauldron") return "大釜を使用";
    if (key == "minecraft:fill_cauldron") return "大釜を満たした";
    if (key == "minecraft:clean_armor") return "大釜で防具を洗った";
    if (key == "minecraft:clean_banner") return "大釜で旗を洗った";
    if (key == "minecraft:clean_shulker_box") return "大釜でシュルカーボックスを洗った";
    if (key == "minecraft:play_record") return "レコードを再生";
    if (key == "minecraft:sleep_in_bed") return "ベッドで眠った";
    if (key == "minecraft:trigger_trapped_chest") return "トラップチェストを作動";
    return key;
}

} // namespace bedrock_insight
