"""Latest observed completed-player-tick counter, never an additive duration."""
SOURCE='ll26.60.player_rest_ticks.v1'
PREDICATE="""
event_kind='player_interaction_result' AND cause='ll26.60.player_rest_ticks.v1'
AND actor_kind='player' AND actor_id!='' AND primary_type='minecraft:time_since_rest' AND secondary_type=''
AND typeof(amount)='integer' AND amount>=0 AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0
AND occurred_at_ms<=:now
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
"""
SQL="""SELECT actor_id,:key AS type,amount AS value,occurred_at_ms AS first_ms,occurred_at_ms AS last_ms
FROM (SELECT *,ROW_NUMBER() OVER(PARTITION BY actor_id ORDER BY occurred_at_ms DESC,event_id DESC) AS rn
FROM events WHERE """+PREDICATE+""" AND occurred_at_ms>=:start AND occurred_at_ms<:end)
WHERE rn=1 AND json_extract(details_json,'$.baseline_known')=1"""
DEFINITION=('同じPlayerで睡眠状態を観測した後の、完了したServerPlayer normalTick呼出し数の最終保存値。'
    '初期値・再接続・30秒超の観測途絶は不明、睡眠中は0。5秒間隔で保存し、期間内の個人別最新値を採用します。'
    '加算・最大値・50ms壁時計換算・オフライン加算ではありません。日/月の期間開始でカウンタを切り直しません。'
    '全員表示は個人別最新値の合計。正の既知値だけを順位化します。過去の保存値で、現在オンラインや熟睡完了は証明しません。')
