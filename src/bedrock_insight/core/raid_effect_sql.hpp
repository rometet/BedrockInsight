#pragma once
namespace bedrock_insight {
inline constexpr char kRaidEffectPredicate[]=R"sql(
event_kind='player_interaction_result' AND cause='ll26.60.raid_effect_changed.v1'
AND actor_kind='player' AND actor_id!='' AND typeof(amount)='integer' AND amount=1
AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0 AND occurred_at_ms<=unixepoch('subsec')*1000
AND CASE WHEN typeof(details_json)='text' AND length(details_json)<=2048 AND json_valid(details_json) THEN (
 json_type(details_json)='object' AND json_extract(details_json,'$.hook')='Actor::addEffect'
 AND json_extract(details_json,'$.phase')='post_origin_effect_change'
 AND json_type(details_json,'$.effect_id')='integer' AND json_type(details_json,'$.duration_ticks')='integer'
 AND ((primary_type='minecraft:raid_trigger' AND secondary_type='effect_id:36'
       AND json_extract(details_json,'$.effect_id')=36 AND json_extract(details_json,'$.duration_ticks')=600)
   OR (primary_type='minecraft:raid_win' AND secondary_type='effect_id:29'
       AND json_extract(details_json,'$.effect_id')=29 AND json_extract(details_json,'$.duration_ticks')=48000))
 AND json_type(details_json,'$.amplifier')='integer' AND json_extract(details_json,'$.amplifier') BETWEEN 0 AND 255
 AND json_type(details_json,'$.after_duration_ticks')='integer'
 AND json_extract(details_json,'$.after_duration_ticks')=json_extract(details_json,'$.duration_ticks')
 AND json_type(details_json,'$.after_amplifier')='integer'
 AND json_extract(details_json,'$.after_amplifier')=json_extract(details_json,'$.amplifier')
 AND json_type(details_json,'$.before_duration_ticks')='integer'
 AND json_type(details_json,'$.before_amplifier')='integer'
 AND ((json_type(details_json,'$.before_present')='false' AND json_extract(details_json,'$.before_duration_ticks')=0
       AND json_extract(details_json,'$.before_amplifier')=0)
   OR (json_type(details_json,'$.before_present')='true' AND json_extract(details_json,'$.before_duration_ticks') BETWEEN -1 AND 2147483647
       AND json_extract(details_json,'$.before_amplifier') BETWEEN 0 AND 255
       AND (json_extract(details_json,'$.before_duration_ticks')!=json_extract(details_json,'$.after_duration_ticks')
         OR json_extract(details_json,'$.before_amplifier')!=json_extract(details_json,'$.after_amplifier'))))
 AND json_type(details_json,'$.same_player')='true' AND json_type(details_json,'$.same_level')='true'
 AND json_type(details_json,'$.nested')='false' AND json_type(details_json,'$.raid_outcome_verified')='false'
 AND json_type(details_json,'$.effect_source_verified')='false' AND json_type(details_json,'$.durable_world_commit_verified')='false'
 AND (SELECT COUNT(*)=COUNT(DISTINCT key) FROM json_each(details_json))
) ELSE 0 END
)sql";
} // namespace bedrock_insight
