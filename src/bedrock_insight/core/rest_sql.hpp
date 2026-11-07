#pragma once
namespace bedrock_insight {
// Unknown snapshots participate in latest selection, suppressing old known
// counters after a gap/reconnect. Zero is a genuine observed reset, not missing.
inline constexpr char kRestPredicate[]=R"sql(
event_kind='player_interaction_result' AND cause='ll26.60.player_rest_ticks.v1'
AND actor_kind='player' AND actor_id!='' AND primary_type='minecraft:time_since_rest' AND secondary_type=''
AND typeof(amount)='integer' AND amount>=0 AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0
AND occurred_at_ms<=unixepoch('subsec')*1000
AND CASE WHEN length(details_json)<=1024 AND json_valid(details_json) THEN (
 json_type(details_json)='object'
 AND json_extract(details_json,'$.hook')='ServerPlayer::$normalTick'
 AND json_extract(details_json,'$.phase')='post_origin_sleep_snapshot'
 AND json_extract(details_json,'$.aggregation')='latest_per_player_snapshot'
 AND json_type(details_json,'$.baseline_known') IN ('true','false')
 AND (json_type(details_json,'$.baseline_known')='true' OR amount=0)
 AND json_type(details_json,'$.ticks')='integer' AND typeof(json_extract(details_json,'$.ticks'))='integer'
 AND json_extract(details_json,'$.ticks')=amount
 AND json_type(details_json,'$.observed_at_ms')='integer' AND typeof(json_extract(details_json,'$.observed_at_ms'))='integer'
 AND json_extract(details_json,'$.observed_at_ms')=occurred_at_ms
 AND json_type(details_json,'$.sample_interval_ms')='integer' AND json_extract(details_json,'$.sample_interval_ms')=5000
 AND json_type(details_json,'$.gap_limit_ms')='integer' AND json_extract(details_json,'$.gap_limit_ms')=30000
 AND json_type(details_json,'$.wall_clock_estimate')='false'
 AND json_type(details_json,'$.offline_time_included')='false'
 AND json_type(details_json,'$.restart_continuity_verified')='false'
 AND json_type(details_json,'$.full_night_sleep_verified')='false'
 AND (SELECT COUNT(*)=COUNT(DISTINCT key) FROM json_each(details_json))
) ELSE 0 END
)sql";
}
