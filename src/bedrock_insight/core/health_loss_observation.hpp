#pragma once
#include "bedrock_insight/core/absorption_observation.hpp"

namespace bedrock_insight {
// Shared numeric/lifetime/reentrancy gates, but an independently sampled
// HEALTH attribute. Never min(input,health), notifications, or absorption.
using HealthLossOutcome=AbsorptionOutcome;
inline std::optional<AnalyticsEvent> health_loss_event(Actor const& credited,std::string_view victim_type,
    bool dealt,HealthLossOutcome const& o,DamageAttributionFacts const& f,std::int64_t at) {
    auto amount=absorption_amount(o);auto path=dealt ? resolved_damage_path(f) : std::string_view("victim");
    if(amount<=0 || credited.kind!=ActorKind::Player || credited.id.empty() || !valid_item_type(victim_type)
        || (!dealt && victim_type!="minecraft:player") || path.empty() || at<0 || f.cause< -1 || f.cause>34)return {};
    AnalyticsEvent v;v.kind=EventKind::PlayerInteractionResult;v.actor=credited;v.occurred_at_ms=at;
    v.primary_type=dealt ? "minecraft:damage_dealt" : "minecraft:damage_taken";
    v.secondary_type=victim_type;v.amount=amount;v.cause="ll26.60.hurt_health_delta.v1";
    v.details_json="{\"hook\":\""+std::string(o.hook==AbsorptionHook::Player ? "Player::$_hurt" : "Mob::$_hurt")
        +"\",\"phase\":\"post_origin_attribute_decrease\",\"result_type\":2,\"attribute\":\"HEALTH\","
        "\"scope\":\"same_actor_same_attribute_instance\",\"rounding\":\"nearest_milli_then_tenth\","
        "\"before_milli\":"+std::to_string(o.before)+",\"after_milli\":"+std::to_string(o.after)
        +",\"input_milli\":"+std::to_string(o.input)+",\"returned_milli\":"+std::to_string(o.returned)
        +",\"parent_calls\":"+std::to_string(o.parent_calls)+",\"same_actor\":true,\"same_attribute\":true,"
        "\"same_level\":true,\"ambiguous\":false,\"role\":\""+(dealt ? "attacker" : "victim")
        +"\",\"victim_type\":\""+std::string(victim_type)+"\",\"attribution\":\""+std::string(path)
        +"\",\"damage_cause\":"+std::to_string(f.cause)+",\"source_id\":"+std::to_string(dealt ? f.source_id : 0)
        +",\"damaging_id\":"+std::to_string(dealt ? f.damaging_id : 0)
        +",\"health_attribute_decrease_observed\":true,\"final_health_loss_measured\":false,"
        "\"exclusive_causation_verified\":false,\"durable_world_commit_verified\":false}";
    return v;
}
} // namespace bedrock_insight
