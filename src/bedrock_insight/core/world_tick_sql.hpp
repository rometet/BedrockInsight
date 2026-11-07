#pragma once
namespace bedrock_insight {
// Read the LAST valid snapshot; NEVER SUM/MAX tick IDs across server runs.
// CASE guards malformed JSON even when SQLite reorders surrounding predicates.
inline constexpr char kWorldTickPredicate[] = R"sql(
event_kind='server_tick' AND cause='ll26.60.world_tick_snapshot.v1'
AND actor_kind='system' AND length(session_id) BETWEEN 1 AND 128 AND actor_id=session_id
AND primary_type='minecraft:total_world_time' AND secondary_type=''
AND typeof(amount)='integer' AND amount>=0
AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0
AND occurred_at_ms<=unixepoch('subsec')*1000
AND CASE WHEN length(details_json)<=1024 AND json_valid(details_json) THEN (
 json_type(details_json)='object'
 AND json_extract(details_json,'$.adapter')='Level::getCurrentServerTick'
 AND json_extract(details_json,'$.phase')='engine_tick_snapshot'
 AND json_extract(details_json,'$.scope')='world_shared'
 AND json_extract(details_json,'$.aggregation')='latest_snapshot'
 AND json_type(details_json,'$.tick_id')='integer' AND typeof(json_extract(details_json,'$.tick_id'))='integer'
 AND json_extract(details_json,'$.tick_id')=amount
 AND json_type(details_json,'$.observed_at_ms')='integer' AND typeof(json_extract(details_json,'$.observed_at_ms'))='integer'
 AND json_extract(details_json,'$.observed_at_ms')=occurred_at_ms
 AND json_type(details_json,'$.sample_interval_ms')='integer' AND json_extract(details_json,'$.sample_interval_ms')=5000
 AND json_type(details_json,'$.player_time')='false'
 AND json_type(details_json,'$.restart_continuity_verified')='false'
 AND (SELECT COUNT(*)=COUNT(DISTINCT key) FROM json_each(details_json))
) ELSE 0 END
)sql";
} // namespace bedrock_insight
