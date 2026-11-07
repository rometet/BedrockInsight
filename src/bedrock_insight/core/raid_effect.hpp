#pragma once
#include "bedrock_insight/core/event.hpp"
#include <optional>
#include <string_view>
#include <utility>

namespace bedrock_insight {
// Named MobEffectIds in the pinned SDK: VillageHero=29, RaidOmen=36.
// These are effect observations, NOT proof that a raid began or was won.
struct RaidEffectState { int id=0, duration=0, amplifier=0; };
struct RaidEffectOutcome {
    RaidEffectState requested, after;
    std::optional<RaidEffectState> before;
    bool same_player=false, same_level=false, nested=false;
};
inline std::string_view raid_effect_key(RaidEffectState const& e) noexcept {
    if(e.amplifier<0 || e.amplifier>255)return {};
    if(e.id==36 && e.duration==600)return "minecraft:raid_trigger";
    if(e.id==29 && e.duration==48000)return "minecraft:raid_win";
    return {};
}
inline bool valid_raid_effect(RaidEffectOutcome const& o) noexcept {
    if(raid_effect_key(o.requested).empty() || !o.same_player || !o.same_level || o.nested
        || o.after.id!=o.requested.id || o.after.duration!=o.requested.duration
        || o.after.amplifier!=o.requested.amplifier)return false;
    if(!o.before)return true;
    auto const& b=*o.before;
    return b.id==o.requested.id && b.duration>=-1 && b.amplifier>=0 && b.amplifier<=255
        && (b.duration!=o.after.duration || b.amplifier!=o.after.amplifier);
}
inline std::optional<AnalyticsEvent> raid_effect_event(Actor const& player,RaidEffectOutcome const& o,std::int64_t at) {
    if(player.kind!=ActorKind::Player || player.id.empty() || at<0 || !valid_raid_effect(o))return {};
    AnalyticsEvent v;v.kind=EventKind::PlayerInteractionResult;v.actor=player;v.occurred_at_ms=at;
    v.primary_type=raid_effect_key(o.requested);v.secondary_type="effect_id:"+std::to_string(o.requested.id);
    v.amount=1;v.cause="ll26.60.raid_effect_changed.v1";
    v.details_json="{\"hook\":\"Actor::addEffect\",\"phase\":\"post_origin_effect_change\","
        "\"effect_id\":"+std::to_string(o.requested.id)+",\"duration_ticks\":"+std::to_string(o.requested.duration)
        +",\"amplifier\":"+std::to_string(o.requested.amplifier)+",\"before_present\":"+(o.before ? "true" : "false")
        +",\"before_duration_ticks\":"+std::to_string(o.before ? o.before->duration : 0)
        +",\"before_amplifier\":"+std::to_string(o.before ? o.before->amplifier : 0)
        +",\"after_duration_ticks\":"+std::to_string(o.after.duration)+",\"after_amplifier\":"+std::to_string(o.after.amplifier)
        +",\"same_player\":true,\"same_level\":true,\"nested\":false,\"raid_outcome_verified\":false,"
        "\"effect_source_verified\":false,\"durable_world_commit_verified\":false}";
    return v;
}
template<class Prepare,class Origin,class Finish,class Emit>
void observe_raid_effect(Prepare prepare,Origin origin,Finish finish,Emit emit) {
    decltype(prepare()) entry;
    try{entry=prepare();}catch(...){}
    origin(); // exactly once; engine exceptions are never swallowed or retried
    if(entry)try{if(auto v=finish(*entry))emit(std::move(*v));}catch(...){}
}
} // namespace bedrock_insight
