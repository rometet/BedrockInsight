#pragma once
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/event.hpp"
#include "bedrock_insight/core/json.hpp"
#include <optional>

namespace bedrock_insight {
inline constexpr char kBrokenV1[]="ll26.60.durability_item_destroyed.v1";
inline constexpr char kBrokenV2[]="ll26.60.durability_item_destroyed.v2";
enum class BrokenLocation {Inventory,Offhand,Armor};
inline std::string_view broken_location_name(BrokenLocation location) noexcept {
    switch(location){case BrokenLocation::Inventory:return "main_inventory";
        case BrokenLocation::Offhand:return "offhand";case BrokenLocation::Armor:return "armor";}
    return {};
}
inline std::string_view broken_location_scope(BrokenLocation location) noexcept {
    switch(location){case BrokenLocation::Inventory:return "main_inventory_same_slot";
        case BrokenLocation::Offhand:return "offhand_same_slot";case BrokenLocation::Armor:return "armor_same_slot";}
    return {};
}
struct BrokenObservation {
    BrokenLocation location=BrokenLocation::Inventory;
    int slots=0,slot=-1,max_damage=0,damage_before=-1,delta_damage=0;
    int entry_units=0,after_units=-1,dimension=0;
};
inline bool broken_frame_valid(bool removed,bool same_player,bool same_level,int before_dimension,int after_dimension) noexcept {
    return !removed && same_player && same_level && before_dimension>=-1 && before_dimension<=127
        && before_dimension==after_dimension;
}
inline bool broken_slot_valid(BrokenObservation const& o) noexcept {
    bool location=false;
    switch(o.location){case BrokenLocation::Inventory:location=o.slots>=1 && o.slots<=128;break;
        case BrokenLocation::Offhand:location=o.slots==1;break;case BrokenLocation::Armor:location=o.slots==4;break;}
    return location && o.slot>=0 && o.slot<o.slots && o.max_damage>1 && o.max_damage<=32767
        && o.damage_before>=0 && o.damage_before<=o.max_damage && o.delta_damage>0
        && std::int64_t(o.damage_before)+o.delta_damage>=o.max_damage && o.entry_units==1 && o.after_units==0
        && o.dimension>=-1 && o.dimension<=127;
}
// The caller has already verified origin's true result, no reentry, and a
// reacquired same-owner/same-location stack. This records an in-call observation,
// not final engine break causation or world persistence.
inline std::optional<AnalyticsEvent> broken_record(Actor const& actor,std::string_view type,
    BrokenObservation const& o,std::int64_t at) {
    if(actor.kind!=ActorKind::Player || actor.id.empty() || actor.id.size()>128 || at<0
        || !valid_item_type(type) || !broken_slot_valid(o))return {};
    AnalyticsEvent v;v.kind=EventKind::ItemBrokenResult;v.actor=actor;v.amount=1;
    v.primary_type=type;v.secondary_type=broken_location_name(o.location);v.cause=kBrokenV2;v.occurred_at_ms=at;
    // No position was read: default XYZ is NOT coordinate evidence.
    v.position.dimension=static_cast<std::int8_t>(o.dimension);
    v.details_json="{\"hook\":\"ItemStackBase::hurtAndBreak\",\"phase\":\"return_true_item_destroyed\",\"scope\":\""
        +std::string(broken_location_scope(o.location))+"\",\"item_type\":\""+std::string(type)
        +"\",\"delta_damage\":"+std::to_string(o.delta_damage)+",\"slots\":"+std::to_string(o.slots)
        +",\"slot\":"+std::to_string(o.slot)+",\"max_damage\":"+std::to_string(o.max_damage)
        +",\"damage_before\":"+std::to_string(o.damage_before)
        +",\"is_damageable\":true,\"entry_units\":1,\"after_units\":0,\"nested\":false,\"durability_path\":true,"
        "\"threshold_input_reached\":true,\"final_break_verified\":false,\"world_save_verified\":false,"
        "\"same_player\":true,\"same_level\":true,\"same_slot\":true,\"dimension\":"+std::to_string(o.dimension)+"}";
    return v;
}
inline bool valid_broken_record(std::string_view raw,std::string_view primary,std::string_view cause,std::string_view secondary) {
    if(!valid_item_type(primary) || (cause!=kBrokenV1 && cause!=kBrokenV2))return false;
    JsonValue d;std::string error;bool const v2=cause==kBrokenV2;
    if(parse_json(raw,d,error,{1024,3,40,256})!=JsonParseResult::Complete || d.type()!=JsonValue::Type::Object
        || d.object().size()!=(v2?21u:15u))return false;
    auto text=[&](std::string_view key,std::string_view val){auto p=d.find(key);return p && p->type()==JsonValue::Type::String && p->string()==val;};
    auto flag=[&](std::string_view key,bool val){auto p=d.find(key);return p && p->type()==JsonValue::Type::Boolean && p->boolean()==val;};
    if(!text("hook","ItemStackBase::hurtAndBreak") || !text("phase","return_true_item_destroyed")
        || !flag("is_damageable",true) || !flag("nested",false) || !flag("durability_path",true))return false;
    BrokenObservation o;
    if(v2) {
        bool found=false;
        for(auto location:{BrokenLocation::Inventory,BrokenLocation::Offhand,BrokenLocation::Armor})
            if(secondary==broken_location_name(location) && text("scope",broken_location_scope(location))){o.location=location;found=true;break;}
        if(!found || !text("item_type",primary) || !flag("same_player",true) || !flag("same_level",true)
            || !flag("same_slot",true) || !flag("threshold_input_reached",true) || !flag("final_break_verified",false)
            || !flag("world_save_verified",false))return false;
    }else if(!secondary.empty() || !text("scope","main_inventory_same_slot")
        || !flag("durability_exhausted_verified",true) || !flag("max_damage_value_exceeded_read",true))return false;
    auto number=[&](std::string_view key,int& out,int low,int high){std::int64_t n=0;
        if(!json_integer_field(d,key,n,true,error) || n<low || n>high)return false;out=static_cast<int>(n);return true;};
    return number("slots",o.slots,1,128) && number("slot",o.slot,0,127)
        && number("max_damage",o.max_damage,2,32767) && number("damage_before",o.damage_before,0,32767)
        && number("delta_damage",o.delta_damage,1,2147483647) && number("entry_units",o.entry_units,1,1)
        && number("after_units",o.after_units,0,0) && (!v2 || number("dimension",o.dimension,-1,127)) && broken_slot_valid(o);
}
inline constexpr char kBrokenRecordPredicate[]=
    "event_kind='item_broken_result' AND actor_kind='player' AND actor_id!='' "
    "AND cause IN ('ll26.60.durability_item_destroyed.v1','ll26.60.durability_item_destroyed.v2') "
    "AND typeof(amount)='integer' AND amount=1 "
    "AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0 AND occurred_at_ms<=CAST(unixepoch('subsec')*1000 AS INTEGER) "
    "AND bi_broken_details(details_json,primary_type,cause,COALESCE(secondary_type,'')) ";
inline constexpr char kBrokenObservationScope[]="耐久処理true応答・入力閾値・同じスロットの1→0観測。v2は主所持品/副手/防具4枠と同Player/Level/次元を照合。旧v1は主所持品のみ。全破損・専属因果・最終保存の証明ではありません。";
}
