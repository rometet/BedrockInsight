#pragma once
#include "bedrock_insight/core/absorption_observation.hpp"
#include <algorithm>

namespace bedrock_insight {
// A versioned formula on an observed stage value, NOT an observed HP delta.
struct ResistanceOutcome {
    AbsorptionHook hook=AbsorptionHook::Mob;
    std::int64_t input=-1,stage_input=-1,basis=-1,returned=-1;
    int amplifier=-1,duration=0,result_type=0,parent_calls=0,stage_calls=0;
    bool same_actor=false,same_level=false,same_source=false,same_effect=false,ambiguous=false;
};
inline std::int64_t resistance_amount(ResistanceOutcome const& o) noexcept {
    if(o.result_type!=2 || o.stage_calls!=1 || o.parent_calls<0 || o.parent_calls>1
        || (o.hook==AbsorptionHook::Mob && o.parent_calls!=0) || o.ambiguous
        || !o.same_actor || !o.same_level || !o.same_source || !o.same_effect
        || o.input<=0 || o.input>100000000 || o.stage_input<=0 || o.stage_input>100000000
        || o.stage_input>o.input+1 || o.basis<=0 || o.basis>o.stage_input+1 || o.basis>100000000
        || o.returned<0 || o.returned>100000000 || o.amplifier<0 || o.amplifier>255 || o.duration<=0)return 0;
    // Levels I..V: 1/5..5/5 of the post-enchantment basis, then nearest tenth.
    return (o.basis*std::min(o.amplifier+1,5)+250)/500;
}
inline std::optional<AnalyticsEvent> resistance_event(Actor const& actor,std::string_view victim_type,
    bool dealt,ResistanceOutcome const& o,DamageAttributionFacts const& f,std::int64_t at) {
    auto value=resistance_amount(o);auto path=dealt ? resolved_damage_path(f) : std::string_view("victim");
    if(value<=0 || actor.kind!=ActorKind::Player || actor.id.empty() || !valid_item_type(victim_type)
        || (!dealt && victim_type!="minecraft:player") || path.empty() || at<0 || f.cause< -1 || f.cause>34)return {};
    AnalyticsEvent v;v.kind=EventKind::PlayerInteractionResult;v.actor=actor;v.occurred_at_ms=at;v.amount=value;
    v.primary_type=dealt ? "minecraft:damage_dealt_resisted" : "minecraft:damage_resisted";
    v.secondary_type=victim_type;v.cause="ll26.60.processed_hurt_resistance_formula.v1";
    v.details_json="{\"hook\":\""+std::string(o.hook==AbsorptionHook::Player ? "Player::$_hurt" : "Mob::$_hurt")
        +"\",\"stage_hook\":\"Mob::$getDamageAfterEnchantReduction\",\"phase\":\"processed_hurt_resistance_formula\","
        "\"calculation\":\"post_enchant_milli_times_capped_level_over_5\",\"rounding\":\"nearest_milli_then_tenth\","
        "\"effect_id\":11,\"amplifier\":"+std::to_string(o.amplifier)+",\"duration_ticks\":"+std::to_string(o.duration)
        +",\"hurt_input_milli\":"+std::to_string(o.input)+",\"stage_input_milli\":"+std::to_string(o.stage_input)
        +",\"basis_milli\":"+std::to_string(o.basis)+",\"returned_milli\":"+std::to_string(o.returned)
        +",\"result_type\":2,\"parent_calls\":"+std::to_string(o.parent_calls)+",\"stage_calls\":1,"
        "\"same_actor\":true,\"same_level\":true,\"same_source\":true,\"same_effect\":true,\"ambiguous\":false,"
        "\"role\":\""+(dealt ? "attacker" : "victim")+"\",\"victim_type\":\""+std::string(victim_type)
        +"\",\"attribution\":\""+std::string(path)+"\",\"damage_cause\":"+std::to_string(f.cause)
        +",\"source_id\":"+std::to_string(dealt ? f.source_id : 0)+",\"damaging_id\":"+std::to_string(dealt ? f.damaging_id : 0)
        +",\"resistance_applied_measured\":false,\"final_health_loss_measured\":false,\"durable_world_commit_verified\":false}";
    return v;
}
template<class Prepare,class Origin,class Finish>
auto observe_resistance(Prepare prepare,Origin origin,Finish finish)->decltype(origin()) {
    decltype(prepare()) entry;try{entry=prepare();}catch(...){}
    auto result=origin();if(entry)try{finish(*entry,result);}catch(...){}return result;
}
// The stage is correlated to the same outer hurt object/source. Duplicate,
// foreign or recursive stage calls poison the frame, never add a second value.
struct ResistanceStageGate {
    void const* actor=nullptr;void const* source=nullptr;int calls=0;bool active=false,ambiguous=false;
    bool enter(void const* a,void const* s) noexcept {
        if(calls<2)++calls;
        if(active || calls!=1 || actor!=a || source!=s){ambiguous=true;return false;}
        active=true;return true;
    }
};
}
