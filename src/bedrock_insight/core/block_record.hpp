#pragma once
#include "bedrock_insight/core/block_observation.hpp"
#include "bedrock_insight/core/event.hpp"
#include "bedrock_insight/core/json.hpp"

namespace bedrock_insight {
struct BlockLocation {std::int32_t x=0,y=0,z=0;int dimension=0;};
// Called only with a newly re-resolved weak Player, before any block-source read.
inline bool block_frame_confirmed(bool removed,bool same_player,bool same_level,int before_dimension,int after_dimension) noexcept {
    return !removed && same_player && same_level && before_dimension>=-1 && before_dimension<=127
        && before_dimension==after_dimension;
}
inline std::optional<AnalyticsEvent> block_transition_record(Actor const& actor,bool placed,
    std::string_view before,std::string_view after,BlockLocation point,int hand,bool sim_tick,std::int64_t at) {
    auto dimension=point.dimension;
    if(actor.kind!=ActorKind::Player || actor.id.empty() || actor.id.size()>128 || at<0
        || dimension< -1 || dimension>127 || point.x<=-30000000 || point.x>=30000000 || point.z<=-30000000 || point.z>=30000000
        || point.y< -512 || point.y>512 || (placed && (hand<0 || hand>1))
        || (!placed && hand!=-1))return {};
    if(placed ? !placed_block_type(before,after).has_value() : !block_break_confirmed(before,after))return {};
    AnalyticsEvent v;v.kind=placed?EventKind::BlockPlacedResult:EventKind::BlockMinedResult;
    v.actor=actor;v.amount=1;v.occurred_at_ms=at;v.primary_type=std::string(placed?after:before);
    v.position={point.x,point.y,point.z,static_cast<std::int8_t>(dimension)};
    v.secondary_type=placed?(hand==0?"main_hand":"off_hand"):"";
    v.cause=placed?"ll26.60.gamemode_build_block.v2":"ll26.60.gamemode_destroy_block.v2";
    // Identifiers are already restricted ASCII; no caller text is interpolated.
    v.details_json=std::string("{\"hook\":\"")+(placed?"GameMode::$buildBlock":"GameMode::$destroyBlock")
        +"\",\"phase\":\"return_true_post_state\",\"scope\":\"same_position_block_source\",\"before_type\":\""
        +std::string(before)+"\",\"after_type\":\""+std::string(after)+"\",\"dimension\":"+std::to_string(dimension)
        +",\"nested\":false,\"state_transition_verified\":true,\"world_save_verified\":false,\"same_player\":true,\"same_level\":true";
    if(placed)v.details_json+=",\"hand\":"+std::to_string(hand)+",\"sim_tick\":"+(sim_tick?"true":"false");
    v.details_json+='}';return v;
}

// Validate an evidence contract, not the truth of an arbitrary edited database.
// v1 remains historical same-position evidence, not retrospective v2 frame proof.
inline bool valid_block_record(std::string_view raw,std::string_view kind,std::string_view primary,std::string_view cause,std::string_view secondary) {
    bool placed=kind=="placed";
    if((kind!="mined" && !placed) || !valid_item_type(primary))return false;
    auto v1=placed?"ll26.60.gamemode_build_block.v1":"ll26.60.gamemode_destroy_block.v1";
    auto v2=placed?"ll26.60.gamemode_build_block.v2":"ll26.60.gamemode_destroy_block.v2";
    if(cause!=v1 && cause!=v2)return false;
    JsonValue d;std::string error;
    if(parse_json(raw,d,error,{768,3,32,256})!=JsonParseResult::Complete || d.type()!=JsonValue::Type::Object
        || d.object().size()!=std::size_t(9+(placed?2:0)+(cause==v2?2:0)))return false;
    auto text=[&](std::string_view key,std::string_view expected){auto p=d.find(key);return p && p->type()==JsonValue::Type::String && p->string()==expected;};
    auto flag=[&](std::string_view key,bool expected){auto p=d.find(key);return p && p->type()==JsonValue::Type::Boolean && p->boolean()==expected;};
    if(!text("hook",placed?"GameMode::$buildBlock":"GameMode::$destroyBlock") || !text("phase","return_true_post_state")
        || !text("scope","same_position_block_source") || !flag("nested",false) || !flag("state_transition_verified",true)
        || !flag("world_save_verified",false) || (cause==v2 && (!flag("same_player",true) || !flag("same_level",true))))return false;
    auto before=d.find("before_type"),after=d.find("after_type");std::int64_t dimension=0;
    if(!before || !after || before->type()!=JsonValue::Type::String || after->type()!=JsonValue::Type::String
        || !json_integer_field(d,"dimension",dimension,true,error) || dimension< -1 || dimension>(cause==v2?127:255))return false;
    if(placed) {
        std::int64_t hand=0;auto sim=d.find("sim_tick");
        return placed_block_type(before->string(),after->string()).has_value() && primary==after->string()
            && json_integer_field(d,"hand",hand,true,error) && hand>=0 && hand<=1 && sim && sim->type()==JsonValue::Type::Boolean
            && secondary==(hand==0?"main_hand":"off_hand");
    }
    return secondary.empty() && block_break_confirmed(before->string(),after->string()) && primary==before->string();
}
inline constexpr char kMinedRecordPredicate[]=
    "event_kind='block_mined_result' AND actor_kind='player' AND actor_id!='' "
    "AND cause IN ('ll26.60.gamemode_destroy_block.v1','ll26.60.gamemode_destroy_block.v2') "
    "AND typeof(amount)='integer' AND amount=1 AND (secondary_type='' OR secondary_type IS NULL) "
    "AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0 AND occurred_at_ms<=CAST(unixepoch('subsec')*1000 AS INTEGER) "
    "AND bi_block_details(details_json,'mined',primary_type,cause,COALESCE(secondary_type,'')) ";
inline constexpr char kBlockObservationScope[]="GameModeのtrue応答と同座標の種類変化のみ。v2は未削除・同Player/Level/次元を照合。旧v1はそのframe証明でなく、全経路・最終永続化・専属因果の証明ではありません。";
}
