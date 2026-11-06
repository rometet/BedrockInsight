#pragma once
#include <cstdint>
#include <string>

namespace bedrock_insight {
// Reused by native summaries and single-key rankings. No running-session or
// offline wall-clock estimates. Merge duplicate/nested/chained closed sessions
// before summing; a simple LAG(end) does not handle nested overlaps.
// clock_ms is a typed internal UTC epoch, never an interpolated caller string.
inline std::string closed_session_times_sql(std::int64_t clock_ms) {
    auto clock=std::to_string(clock_ms);
    return "WITH valid AS (SELECT player_id,session_id,started_at_ms AS b,ended_at_ms AS e "
        "FROM sessions WHERE typeof(started_at_ms)='integer' AND typeof(ended_at_ms)='integer' "
        "AND typeof(duration_ms)='integer' AND started_at_ms>=0 AND ended_at_ms>started_at_ms "
        "AND ended_at_ms<="+clock+" AND duration_ms=ended_at_ms-started_at_ms), "
        "ordered AS (SELECT *,MAX(e) OVER(PARTITION BY player_id ORDER BY b,e,session_id "
        "ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) AS prior_e FROM valid), "
        "grouped AS (SELECT *,SUM(CASE WHEN prior_e IS NULL OR b>prior_e THEN 1 ELSE 0 END) "
        "OVER(PARTITION BY player_id ORDER BY b,e,session_id ROWS UNBOUNDED PRECEDING) AS g FROM ordered), "
        "merged AS (SELECT player_id,MIN(b) AS b,MAX(e) AS e FROM grouped GROUP BY player_id,g), "
        "death AS (SELECT actor_id,MAX(occurred_at_ms) AS last_ms FROM events "
        "WHERE event_kind='player_death' AND cause='ll26.60.player_dead_transition.v1' "
        "AND actor_kind='player' AND primary_type='minecraft:player' "
        "AND typeof(amount)='integer' AND amount=1 AND typeof(occurred_at_ms)='integer' "
        "AND occurred_at_ms>=0 AND occurred_at_ms<="+clock+" GROUP BY actor_id), "
        "times AS (SELECT m.player_id,SUM(e-b) AS play_ms, "
        "SUM(MAX(0,e-MAX(b,COALESCE(d.last_ms,0)))) AS since_death_ms "
        "FROM merged m LEFT JOIN death d ON d.actor_id=m.player_id GROUP BY m.player_id) ";
}
}
