#pragma once
#include "bedrock_insight/core/event.hpp"
#include <limits>
#include <optional>

namespace bedrock_insight {
// Counts completed PLAYER tick calls, not wall time or Level tick callbacks.
// An unknown baseline stays unknown until sleeping is actually observed.
struct RestCounter {
    std::int64_t ticks=0,last_us=-1,emitted_us=-1;
    bool known=false;
    bool observe(bool sleeping,std::int64_t us) noexcept {
        if(us<0)return false;
        if(last_us<0 || us<last_us || us-last_us>30000000) {known=false;ticks=0;emitted_us=-1;}
        last_us=us;
        if(sleeping){known=true;ticks=0;}
        else if(known) {
            if(ticks==std::numeric_limits<std::int64_t>::max()){known=false;ticks=0;}
            else ++ticks;
        }
        if(emitted_us>=0 && us>=emitted_us && us-emitted_us<5000000)return false;
        emitted_us=us;return true;
    }
};
inline std::optional<AnalyticsEvent> rest_snapshot(Actor const& actor,RestCounter const& c,std::int64_t at) {
    if(actor.kind!=ActorKind::Player || actor.id.empty() || at<0 || c.ticks<0 || (!c.known && c.ticks!=0))return {};
    AnalyticsEvent v;v.kind=EventKind::PlayerInteractionResult;v.actor=actor;v.occurred_at_ms=at;
    v.primary_type="minecraft:time_since_rest";v.cause="ll26.60.player_rest_ticks.v1";v.amount=c.ticks;
    v.details_json="{\"hook\":\"ServerPlayer::$normalTick\",\"phase\":\"post_origin_sleep_snapshot\","
        "\"aggregation\":\"latest_per_player_snapshot\",\"baseline_known\":"+std::string(c.known?"true":"false")
        +",\"ticks\":"+std::to_string(c.ticks)+",\"observed_at_ms\":"+std::to_string(at)
        +",\"sample_interval_ms\":5000,\"gap_limit_ms\":30000,\"wall_clock_estimate\":false,"
        "\"offline_time_included\":false,\"restart_continuity_verified\":false,\"full_night_sleep_verified\":false}";
    return v;
}
// Observation failures must not skip/repeat origin or swallow engine exceptions.
template<class Origin,class Observe>
void observe_rest_tick(Origin origin,Observe observe){origin();try{observe();}catch(...){} }
}
