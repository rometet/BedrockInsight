#pragma once
namespace bedrock_insight {
// Shared native personal/ranking predicate. CASE is essential: malformed JSON
// must not throw merely because SQLite reorders other WHERE terms.
inline constexpr char kKilledByPredicate[] =
    "event_kind='player_killed_by_entity' AND cause='ll26.60.resolved_killer_dead_transition.v1' "
    "AND actor_kind='player' AND actor_id!='' AND typeof(amount)='integer' AND amount=1 "
    "AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0 "
    "AND occurred_at_ms<=unixepoch('subsec')*1000 "
    "AND length(primary_type) BETWEEN 3 AND 128 AND instr(primary_type,':')>1 "
    "AND instr(primary_type,':')<length(primary_type) AND primary_type NOT GLOB '*[^a-z0-9_.:/-]*' "
    "AND instr(substr(primary_type,instr(primary_type,':')+1),':')=0 "
    "AND instr(substr(primary_type,1,instr(primary_type,':')-1),'/')=0 "
    "AND CASE WHEN length(details_json)<=1024 AND json_valid(details_json) THEN ("
    "json_type(details_json)='object' "
    "AND json_extract(details_json,'$.hook')='Player::$die' "
    "AND json_extract(details_json,'$.phase')='post_dead_transition' "
    "AND json_type(details_json,'$.before_dead')='false' AND json_type(details_json,'$.after_dead')='true' "
    "AND json_type(details_json,'$.nested')='false' AND json_type(details_json,'$.source_entity')='true' "
    "AND json_type(details_json,'$.source_block')='false' AND json_type(details_json,'$.resolved_actor')='true' "
    "AND json_type(details_json,'$.same_level')='true' AND json_type(details_json,'$.self')='false' "
    "AND json_type(details_json,'$.world_death_verified')='false' "
    "AND json_type(details_json,'$.source_id')='integer' AND json_extract(details_json,'$.source_id')!=0 "
    "AND typeof(json_extract(details_json,'$.source_id'))='integer' "
    "AND json_type(details_json,'$.damaging_id')='integer' AND json_extract(details_json,'$.damaging_id')!=0 "
    "AND typeof(json_extract(details_json,'$.damaging_id'))='integer' "
    "AND json_type(details_json,'$.damage_cause')='integer' "
    "AND ((secondary_type='direct' AND json_extract(details_json,'$.damage_cause')=2 "
    "AND json_type(details_json,'$.source_child')='false' "
    "AND json_extract(details_json,'$.source_id')=json_extract(details_json,'$.damaging_id')) "
    "OR (secondary_type='child_projectile' AND json_extract(details_json,'$.damage_cause')=3 "
    "AND json_type(details_json,'$.source_child')='true' "
    "AND json_extract(details_json,'$.source_id')!=json_extract(details_json,'$.damaging_id'))) "
    // Duplicate keys can otherwise have different first/last-key semantics in
    // SQLite versus the Python viewer. Reject the ambiguous object entirely.
    "AND (SELECT COUNT(*)=COUNT(DISTINCT key) FROM json_each(details_json))) ELSE 0 END";
} // namespace bedrock_insight
