#pragma once
#include "bedrock_insight/core/event.hpp"
#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace bedrock_insight {
// Public content IDs, not engine numeric actor IDs or a translated-name match.
struct FishBucketType { std::string_view actor, item; };
inline constexpr std::array<FishBucketType, 7> kFishBucketTypes{{
    {"minecraft:cod", "minecraft:cod_bucket"},
    {"minecraft:fish", "minecraft:cod_bucket"}, // legacy Bedrock cod identifier
    {"minecraft:salmon", "minecraft:salmon_bucket"},
    {"minecraft:pufferfish", "minecraft:pufferfish_bucket"},
    {"minecraft:tropicalfish", "minecraft:tropical_fish_bucket"},
    {"minecraft:axolotl", "minecraft:axolotl_bucket"},
    {"minecraft:tadpole", "minecraft:tadpole_bucket"},
}};
inline std::string_view fish_bucket_item(std::string_view actor) noexcept {
    for (auto const& pair : kFishBucketTypes) if (pair.actor == actor) return pair.item;
    return {};
}
struct FishBucketOutcome {
    bool interaction_success=false, bucket_swing=false, target_retired=false, target_unavailable=false;
    bool same_player=false, same_inventory=false, same_dimension=false, nested=false;
    int bucket_calls=0, slots_before=0, slots_after=0;
    std::int64_t before_units=-1, after_units=-1;
};
inline bool valid_fish_bucket_outcome(FishBucketOutcome const& o) noexcept {
    return o.interaction_success && o.bucket_swing && o.target_retired && o.same_player
        && o.same_inventory && o.same_dimension && !o.nested && o.bucket_calls==1
        && o.slots_before>=1 && o.slots_before<=128 && o.slots_after==o.slots_before
        && o.before_units>=0 && o.before_units<static_cast<std::int64_t>(o.slots_after)*255
        && o.after_units>=0 && o.after_units<=static_cast<std::int64_t>(o.slots_after)*255
        && o.after_units==o.before_units+1;
}
inline std::optional<AnalyticsEvent> fish_bucket_event(Actor const& player, std::string_view type,
    FishBucketOutcome const& o, std::int64_t at) {
    auto const item=fish_bucket_item(type);
    if (player.kind!=ActorKind::Player || player.id.empty() || item.empty() || at<0 || !valid_fish_bucket_outcome(o)) return {};
    AnalyticsEvent value;
    value.kind=EventKind::PlayerInteractionResult;value.actor=player;value.occurred_at_ms=at;
    value.primary_type="minecraft:fish_caught";value.secondary_type=type;value.amount=1;
    value.cause="ll26.60.fish_bucket_delta.v1";
    value.details_json="{\"hook\":\"Player::interact\",\"child_hook\":\"BucketItem::$_useOn\","
        "\"phase\":\"return_success_bucket_gain_target_retired\",\"scope\":\"main_inventory_type_total\","
        "\"target_type\":\""+std::string(type)+"\",\"bucket_type\":\""+std::string(item)+"\","
        "\"hand\":\"main_hand\",\"entry_bucket_type\":\"minecraft:water_bucket\",\"entry_bucket_units\":1,"
        "\"interaction_success\":true,\"bucket_swing\":true,\"target_retired\":true,\"target_after\":\""
        +std::string(o.target_unavailable ? "weak_unavailable" : "removed_flag")+"\","
        "\"same_call_target\":true,\"bucket_calls\":1,\"nested\":false,\"slots\":"+std::to_string(o.slots_after)
        +",\"before_units\":"+std::to_string(o.before_units)+",\"after_units\":"+std::to_string(o.after_units)
        +",\"fishing_rod\":false,\"durable_world_commit_verified\":false}";
    return value;
}
// Exactly-once origin, preserved return/exception. Telemetry failure is isolated.
template<class Prepare,class Origin,class Finish,class Emit>
auto observe_fish_interaction(Prepare prepare,Origin origin,Finish finish,Emit emit) -> decltype(origin()) {
    decltype(prepare()) entry;
    try { entry=prepare(); } catch (...) {}
    auto result=origin(); // deliberately outside telemetry's exception boundary
    if (entry) try { if (auto value=finish(*entry,result)) emit(std::move(*value)); } catch (...) {}
    return result;
}
} // namespace bedrock_insight
