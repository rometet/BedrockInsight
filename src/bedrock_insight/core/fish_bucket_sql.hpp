#pragma once
namespace bedrock_insight {
inline constexpr char kFishBucketPredicate[] = R"sql(
event_kind='player_interaction_result' AND cause='ll26.60.fish_bucket_delta.v1'
AND actor_kind='player' AND actor_id!='' AND primary_type='minecraft:fish_caught'
AND typeof(amount)='integer' AND amount=1 AND typeof(occurred_at_ms)='integer'
AND occurred_at_ms>=0 AND occurred_at_ms<=unixepoch('subsec')*1000
AND CASE WHEN typeof(details_json)='text' AND length(details_json)<=2048 AND json_valid(details_json) THEN (
 json_type(details_json)='object'
 AND json_extract(details_json,'$.hook')='Player::interact'
 AND json_extract(details_json,'$.child_hook')='BucketItem::$_useOn'
 AND json_extract(details_json,'$.phase')='return_success_bucket_gain_target_retired'
 AND json_extract(details_json,'$.scope')='main_inventory_type_total'
 AND json_extract(details_json,'$.target_type')=secondary_type
 AND json_extract(details_json,'$.bucket_type')=CASE secondary_type
 WHEN 'minecraft:cod' THEN 'minecraft:cod_bucket' WHEN 'minecraft:fish' THEN 'minecraft:cod_bucket'
 WHEN 'minecraft:salmon' THEN 'minecraft:salmon_bucket' WHEN 'minecraft:pufferfish' THEN 'minecraft:pufferfish_bucket'
 WHEN 'minecraft:tropicalfish' THEN 'minecraft:tropical_fish_bucket' WHEN 'minecraft:axolotl' THEN 'minecraft:axolotl_bucket'
 WHEN 'minecraft:tadpole' THEN 'minecraft:tadpole_bucket' ELSE NULL END
 AND json_extract(details_json,'$.hand')='main_hand'
 AND json_extract(details_json,'$.entry_bucket_type')='minecraft:water_bucket'
 AND json_type(details_json,'$.entry_bucket_units')='integer' AND json_extract(details_json,'$.entry_bucket_units')=1
 AND json_type(details_json,'$.interaction_success')='true'
 AND json_type(details_json,'$.bucket_swing')='true' AND json_type(details_json,'$.target_retired')='true'
 AND json_extract(details_json,'$.target_after') IN ('removed_flag','weak_unavailable')
 AND json_type(details_json,'$.same_call_target')='true'
 AND json_type(details_json,'$.bucket_calls')='integer' AND json_extract(details_json,'$.bucket_calls')=1
 AND json_type(details_json,'$.nested')='false'
 AND json_type(details_json,'$.slots')='integer' AND json_extract(details_json,'$.slots') BETWEEN 1 AND 128
 AND json_type(details_json,'$.before_units')='integer' AND json_extract(details_json,'$.before_units')>=0
 AND json_extract(details_json,'$.before_units')<json_extract(details_json,'$.slots')*255
 AND json_type(details_json,'$.after_units')='integer'
 AND json_extract(details_json,'$.after_units')=json_extract(details_json,'$.before_units')+1
 AND json_extract(details_json,'$.after_units')<=json_extract(details_json,'$.slots')*255
 AND json_type(details_json,'$.fishing_rod')='false'
 AND json_type(details_json,'$.durable_world_commit_verified')='false'
 AND (SELECT COUNT(*)=COUNT(DISTINCT key) FROM json_each(details_json))
) ELSE 0 END
)sql";
} // namespace bedrock_insight
