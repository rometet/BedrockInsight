#pragma once
namespace bedrock_insight {
inline constexpr char kResistancePredicate[]=R"sql(
event_kind='player_interaction_result' AND cause='ll26.60.processed_hurt_resistance_formula.v1'
AND actor_kind='player' AND actor_id!='' AND typeof(amount)='integer' AND amount BETWEEN 1 AND 1000000
AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0 AND occurred_at_ms<=unixepoch('subsec')*1000
AND CASE WHEN typeof(details_json)='text' AND length(details_json)<=2048 AND json_valid(details_json) THEN (
 json_type(details_json)='object' AND json_extract(details_json,'$.hook') IN ('Player::$_hurt','Mob::$_hurt')
 AND json_extract(details_json,'$.stage_hook')='Mob::$getDamageAfterEnchantReduction'
 AND json_extract(details_json,'$.phase')='processed_hurt_resistance_formula'
 AND json_extract(details_json,'$.calculation')='post_enchant_milli_times_capped_level_over_5'
 AND json_extract(details_json,'$.rounding')='nearest_milli_then_tenth'
 AND json_type(details_json,'$.effect_id')='integer' AND json_extract(details_json,'$.effect_id')=11
 AND json_type(details_json,'$.amplifier')='integer' AND json_extract(details_json,'$.amplifier') BETWEEN 0 AND 255
 AND json_type(details_json,'$.duration_ticks')='integer' AND json_extract(details_json,'$.duration_ticks') BETWEEN 1 AND 2147483647
 AND json_type(details_json,'$.hurt_input_milli')='integer' AND json_extract(details_json,'$.hurt_input_milli') BETWEEN 1 AND 100000000
 AND json_type(details_json,'$.stage_input_milli')='integer' AND json_extract(details_json,'$.stage_input_milli') BETWEEN 1 AND 100000000
 AND json_extract(details_json,'$.stage_input_milli')<=json_extract(details_json,'$.hurt_input_milli')+1
 AND json_type(details_json,'$.basis_milli')='integer' AND json_extract(details_json,'$.basis_milli') BETWEEN 1 AND 100000000
 AND json_extract(details_json,'$.basis_milli')<=json_extract(details_json,'$.stage_input_milli')+1
 AND json_type(details_json,'$.returned_milli')='integer' AND json_extract(details_json,'$.returned_milli') BETWEEN 0 AND 100000000
 AND amount=CAST((json_extract(details_json,'$.basis_milli')*MIN(json_extract(details_json,'$.amplifier')+1,5)+250)/500 AS INTEGER)
 AND json_type(details_json,'$.result_type')='integer' AND json_extract(details_json,'$.result_type')=2
 AND json_type(details_json,'$.stage_calls')='integer' AND json_extract(details_json,'$.stage_calls')=1
 AND json_type(details_json,'$.parent_calls')='integer' AND json_extract(details_json,'$.parent_calls') BETWEEN 0 AND 1
 AND (json_extract(details_json,'$.hook')='Player::$_hurt' OR json_extract(details_json,'$.parent_calls')=0)
 AND json_type(details_json,'$.same_actor')='true' AND json_type(details_json,'$.same_level')='true'
 AND json_type(details_json,'$.same_source')='true' AND json_type(details_json,'$.same_effect')='true'
 AND json_type(details_json,'$.ambiguous')='false'
 AND json_extract(details_json,'$.victim_type')=secondary_type AND length(secondary_type) BETWEEN 3 AND 128
 AND instr(secondary_type,':')>1 AND instr(secondary_type,':')<length(secondary_type)
 AND length(secondary_type)-length(replace(secondary_type,':',''))=1
 AND substr(secondary_type,1,instr(secondary_type,':')-1) NOT GLOB '*[^a-z0-9_.-]*'
 AND substr(secondary_type,instr(secondary_type,':')+1) NOT GLOB '*[^a-z0-9_./-]*'
 AND json_type(details_json,'$.damage_cause')='integer' AND json_extract(details_json,'$.damage_cause') BETWEEN -1 AND 34
 AND json_type(details_json,'$.source_id')='integer' AND json_type(details_json,'$.damaging_id')='integer'
 AND typeof(json_extract(details_json,'$.source_id'))='integer' AND typeof(json_extract(details_json,'$.damaging_id'))='integer'
 AND ((primary_type='minecraft:damage_resisted' AND secondary_type='minecraft:player'
       AND json_extract(details_json,'$.role')='victim' AND json_extract(details_json,'$.attribution')='victim'
       AND json_extract(details_json,'$.source_id')=0 AND json_extract(details_json,'$.damaging_id')=0)
   OR (primary_type='minecraft:damage_dealt_resisted' AND json_extract(details_json,'$.role')='attacker'
       AND json_extract(details_json,'$.source_id')!=0 AND json_extract(details_json,'$.damaging_id')!=0
       AND ((json_extract(details_json,'$.attribution')='direct' AND json_extract(details_json,'$.damage_cause')=2
             AND json_extract(details_json,'$.source_id')=json_extract(details_json,'$.damaging_id'))
         OR (json_extract(details_json,'$.attribution')='child_projectile' AND json_extract(details_json,'$.damage_cause')=3
             AND json_extract(details_json,'$.source_id')!=json_extract(details_json,'$.damaging_id')))))
 AND json_type(details_json,'$.resistance_applied_measured')='false'
 AND json_type(details_json,'$.final_health_loss_measured')='false'
 AND json_type(details_json,'$.durable_world_commit_verified')='false'
 AND (SELECT COUNT(*)=COUNT(DISTINCT key) FROM json_each(details_json))
) ELSE 0 END
)sql";
}
