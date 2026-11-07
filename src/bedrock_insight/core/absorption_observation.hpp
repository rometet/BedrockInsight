#pragma once
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/damage_attribution.hpp"
#include <cmath>
#include <optional>

namespace bedrock_insight {
// Fixed-point observation, not min(notified_damage, absorption) or HP loss.
inline std::optional<std::int64_t> absorption_milli(float value) noexcept {
    if(!std::isfinite(value) || value<0 || value>100000)return {};
    return static_cast<std::int64_t>(std::llround(static_cast<double>(value)*1000));
}
enum class AbsorptionHook { Player, Mob };
struct AbsorptionCall {
    void const* identity=nullptr;AbsorptionHook hook=AbsorptionHook::Mob;
    int parent_calls=0;bool in_parent=false,ambiguous=false;
    bool admit_parent(void const* actor,AbsorptionHook child) noexcept {
        if(hook==AbsorptionHook::Player && child==AbsorptionHook::Mob && identity==actor
            && parent_calls==0 && !in_parent){++parent_calls;return true;}
        ambiguous=true;return false;
    }
};
struct AbsorptionOutcome {
    AbsorptionHook hook=AbsorptionHook::Mob;
    std::int64_t before=-1,after=-1,input=-1,returned=-1;
    int result_type=0,parent_calls=0;
    bool same_actor=false,same_attribute=false,same_level=false,ambiguous=false;
};
inline std::int64_t absorption_amount(AbsorptionOutcome const& o) noexcept {
    if(o.result_type!=2 || !o.same_actor || !o.same_attribute || !o.same_level || o.ambiguous
        || o.parent_calls<0 || o.parent_calls>1 || (o.hook==AbsorptionHook::Mob && o.parent_calls!=0)
        || o.before<0 || o.before>100000000 || o.after<0 || o.after>=o.before
        || o.input<=0 || o.input>100000000 || o.before-o.after>o.input+1
        || o.returned<0 || o.returned>100000000)return 0;
    return (o.before-o.after+50)/100; // nearest milli first, then nearest 1/10 HP
}
inline std::optional<AnalyticsEvent> absorption_event(Actor const& credited,std::string_view victim_type,
    bool dealt,AbsorptionOutcome const& o,DamageAttributionFacts const& f,std::int64_t at) {
    auto amount=absorption_amount(o);auto path=dealt ? resolved_damage_path(f) : std::string_view("victim");
    if(amount<=0 || credited.kind!=ActorKind::Player || credited.id.empty() || !valid_item_type(victim_type)
        || (!dealt && victim_type!="minecraft:player") || path.empty() || at<0 || f.cause< -1 || f.cause>34)return {};
    AnalyticsEvent v;v.kind=EventKind::PlayerInteractionResult;v.actor=credited;v.occurred_at_ms=at;
    v.primary_type=dealt ? "minecraft:damage_dealt_absorbed" : "minecraft:damage_absorbed";
    v.secondary_type=victim_type;v.amount=amount;v.cause="ll26.60.hurt_absorption_delta.v1";
    v.details_json="{\"hook\":\""+std::string(o.hook==AbsorptionHook::Player ? "Player::$_hurt" : "Mob::$_hurt")
        +"\",\"phase\":\"post_origin_attribute_decrease\",\"result_type\":2,\"attribute\":\"ABSORPTION\","
        "\"scope\":\"same_actor_same_attribute_instance\",\"rounding\":\"nearest_milli_then_tenth\","
        "\"before_milli\":"+std::to_string(o.before)+",\"after_milli\":"+std::to_string(o.after)
        +",\"input_milli\":"+std::to_string(o.input)+",\"returned_milli\":"+std::to_string(o.returned)
        +",\"parent_calls\":"+std::to_string(o.parent_calls)+",\"same_actor\":true,\"same_attribute\":true,"
        "\"same_level\":true,\"ambiguous\":false,\"role\":\""+(dealt ? "attacker" : "victim")
        +"\",\"victim_type\":\""+std::string(victim_type)+"\",\"attribution\":\""+std::string(path)
        +"\",\"damage_cause\":"+std::to_string(f.cause)+",\"source_id\":"+std::to_string(dealt ? f.source_id : 0)
        +",\"damaging_id\":"+std::to_string(dealt ? f.damaging_id : 0)
        +",\"final_health_loss_measured\":false,\"exclusive_causation_verified\":false,\"durable_world_commit_verified\":false}";
    return v;
}
template<class Prepare,class Origin,class Finish,class Emit>
auto observe_absorption(Prepare prepare,Origin origin,Finish finish,Emit emit)->decltype(origin()) {
    decltype(prepare()) entry;try{entry=prepare();}catch(...){}
    auto result=origin();
    if(entry)try{finish(*entry,result,emit);}catch(...){}
    return result;
}
} // namespace bedrock_insight
