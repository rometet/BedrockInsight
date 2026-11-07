#pragma once
#include "bedrock_insight/core/absorption_observation.hpp"
#include <unordered_map>

namespace bedrock_insight {
inline bool shield_evidence_opt_in(char const* raw) noexcept {return raw && raw[0]=='1' && raw[1]=='\0';}
struct ShieldFields {
    bool blocking=false,blocked_flag=false,previous_blocked=false,previous_damaged_blocked=false;
    bool invulnerable=false,dead=false;
    std::int64_t shield_time=0,damaged_shield_time=0;
    std::int32_t invulnerable_ticks=0;
};
struct ShieldEvidence {
    ShieldFields before,after;
    std::optional<std::int64_t> input,returned;
    int result_type=0,cause=-1;
    bool may_block=false,invulnerable_to_source=false,returned_present=false,ambiguous=false;
};
// Limits raw investigation rows, not gameplay or statistics. No actor pointer.
class ShieldEvidenceRate {
    std::unordered_map<std::string,std::int64_t> last_;
public:
    bool admit(std::string const& id,std::int64_t us) {
        if(id.empty() || id.size()>128 || us<0)return false;
        auto it=last_.find(id);
        if(it!=last_.end()) {
            if(us>=it->second && us-it->second<5000000)return false;
            it->second=us;return true;
        }
        if(last_.size()>=256) {
            for(auto p=last_.begin();p!=last_.end();) {
                if(us>=p->second && us-p->second>30000000)p=last_.erase(p);else ++p;
            }
            if(last_.size()>=256)return false;
        }
        last_.emplace(id,us);return true;
    }
};
inline std::string shield_fields_json(ShieldFields const& f) {
    auto b=[](bool v){return v?"true":"false";};
    return std::string("{\"blocking\":")+b(f.blocking)+",\"blocked_flag\":"+b(f.blocked_flag)
        +",\"previous_blocked\":"+b(f.previous_blocked)+",\"previous_damaged_blocked\":"+b(f.previous_damaged_blocked)
        +",\"invulnerable\":"+b(f.invulnerable)+",\"dead\":"+b(f.dead)
        +",\"shield_time\":"+std::to_string(f.shield_time)+",\"damaged_shield_time\":"+std::to_string(f.damaged_shield_time)
        +",\"invulnerable_ticks\":"+std::to_string(f.invulnerable_ticks)+"}";
}
inline std::optional<AnalyticsEvent> shield_evidence_event(Actor const& actor,ShieldEvidence const& e,std::int64_t at) {
    auto valid=[](auto const& v){return !v || (*v>=0 && *v<=100000000);};
    if(actor.kind!=ActorKind::Player || actor.id.empty() || at<0 || e.result_type<0 || e.result_type>2
        || !valid(e.input) || !valid(e.returned) || (!e.returned_present && e.returned))return {};
    auto b=[](bool v){return v?"true":"false";};
    auto n=[](auto const& v){return v?std::to_string(*v):std::string("null");};
    AnalyticsEvent v;v.kind=EventKind::PlayerShieldEvidence;v.actor=actor;v.occurred_at_ms=at;
    v.primary_type="minecraft:shield_evidence";v.amount=0;v.cause="ll26.60.player_hurt_shield_evidence.v1";
    v.details_json=std::string("{\"hook\":\"Player::$_hurt\",\"phase\":\"post_origin_raw_fields\",\"input_milli\":")+n(e.input)
        +",\"result_type\":"+std::to_string(e.result_type)+",\"damage_cause\":"+std::to_string(e.cause)
        +",\"returned_milli\":"+n(e.returned)+",\"returned_present\":"+b(e.returned_present)
        +",\"may_block\":"+b(e.may_block)+",\"invulnerable_to_source\":"+b(e.invulnerable_to_source)
        +",\"before\":"+shield_fields_json(e.before)+",\"after\":"+shield_fields_json(e.after)
        +",\"same_player\":true,\"same_level\":true,\"ambiguous\":"+b(e.ambiguous)
        +",\"sample_interval_ms\":5000,\"max_tracked_players\":256,\"rounding\":\"nearest_milli\","
        "\"shield_success_verified\":false,\"shield_blocked_amount_measured\":false,\"eligible_for_statistics\":false}";
    return v;
}
template<class Prepare,class Origin,class Finish>
auto observe_shield_evidence(Prepare prepare,Origin origin,Finish finish)->decltype(origin()) {
    decltype(prepare()) entry;try{entry=prepare();}catch(...){}
    auto result=origin();if(entry)try{finish(*entry,result);}catch(...){}return result;
}
}
