#pragma once
namespace bedrock_insight {
// Legacy rows retain their original meaning. v2 is admitted only with its
// bounded native window contract. CASE protects json_extract from malformed JSON.
inline constexpr char kTravelPredicate[] =
    "event_kind='player_travel_distance' AND actor_kind='player' AND actor_id!='' "
    "AND typeof(amount)='integer' AND amount>0 AND typeof(occurred_at_ms)='integer' "
    "AND occurred_at_ms>=0 AND occurred_at_ms<=unixepoch('subsec')*1000 "
    "AND (cause='ll26.60.poll_position_delta.v1' OR (cause='ll26.60.poll_position_delta.v2' "
    "AND amount<=100000000 AND primary_type IN ('walk','sprint','crouch','swim','fly','aviate','fall','climb',"
    "'boat','minecart','pig','strider','horse','nautilus','happy_ghast','walk_on_water','walk_under_water') "
    "AND CASE WHEN length(details_json)<=2048 AND json_valid(details_json) THEN ("
    "json_type(details_json)='object' AND json_extract(details_json,'$.adapter')='ServerLevelTickEvent' "
    "AND json_extract(details_json,'$.phase')='coherent_position_window' "
    "AND json_extract(details_json,'$.clock')='steady' AND json_extract(details_json,'$.mode')=primary_type "
    "AND json_type(details_json,'$.distance_cm')='integer' AND typeof(json_extract(details_json,'$.distance_cm'))='integer' "
    "AND json_extract(details_json,'$.distance_cm')=amount "
    "AND json_type(details_json,'$.first_ms')='integer' AND typeof(json_extract(details_json,'$.first_ms'))='integer' "
    "AND json_type(details_json,'$.last_ms')='integer' AND typeof(json_extract(details_json,'$.last_ms'))='integer' "
    "AND json_extract(details_json,'$.first_ms')>=0 AND json_extract(details_json,'$.first_ms')<=occurred_at_ms "
    "AND json_extract(details_json,'$.last_ms')=occurred_at_ms "
    "AND (json_extract(details_json,'$.first_ms')/86400000 + (json_extract(details_json,'$.first_ms')%86400000+32400000)/86400000)="
    "(occurred_at_ms/86400000 + (occurred_at_ms%86400000+32400000)/86400000) "
    "AND json_type(details_json,'$.dimension')='integer' AND json_extract(details_json,'$.dimension') BETWEEN -2147483648 AND 2147483647 "
    "AND json_type(details_json,'$.sample_count')='integer' AND json_extract(details_json,'$.sample_count') BETWEEN 1 AND 1000000 "
    "AND json_type(details_json,'$.flush_interval_ms')='integer' AND json_extract(details_json,'$.flush_interval_ms')=5000 "
    "AND json_type(details_json,'$.max_gap_us')='integer' AND json_extract(details_json,'$.max_gap_us')=1000000 "
    "AND json_type(details_json,'$.max_step_cm')='integer' AND json_extract(details_json,'$.max_step_cm')=1000 "
    "AND json_extract(details_json,'$.precision')='floor_window_cm' "
    "AND json_extract(details_json,'$.period_assignment')='last_observation' "
    "AND json_type(details_json,'$.world_movement_verified')='false' "
    "AND (SELECT COUNT(*)=COUNT(DISTINCT key) FROM json_each(details_json))) ELSE 0 END))";
} // namespace bedrock_insight
