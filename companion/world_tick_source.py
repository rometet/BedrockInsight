"""Engine WORLD snapshot contract, independent of player/time/TPS estimates."""
import sqlite3

SOURCE = 'll26.60.world_tick_snapshot.v1'
# Mirrored native predicate, equivalence tested on malformed and valid rows.
PREDICATE = """
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
"""


def report(database, analytics, *, catalog, key, unit, selected_label, label, args,
           player, page, rank_page, players_page, limit, now_ms, definition):
    from .analytics import snapshot
    with snapshot(database) as db:
        # Old DB schemas without a run identifier cannot prove provenance.
        columns={row['name'] for row in db.execute('PRAGMA table_info(events)')}
        row=None
        if 'session_id' in columns:
            sql='SELECT amount,occurred_at_ms,session_id FROM events WHERE '+PREDICATE.replace("unixepoch('subsec')*1000",':now')
            sql+=''' AND occurred_at_ms>=:start AND occurred_at_ms<:end
                AND (:type IS NULL OR :type=:key)
                AND (instr(:key,:search)>0 OR instr(:label,:search)>0)
                ORDER BY occurred_at_ms DESC,event_id DESC LIMIT 1'''
            row=db.execute(sql,args|{'label':selected_label}).fetchone()
        selected=None
        if player:
            for i,p in enumerate(db.execute('SELECT player_id,last_name FROM players ORDER BY player_id LIMIT 100001')):
                if i==100000:raise sqlite3.OperationalError('player lookup exceeds budget')
                if analytics.player_key(p['player_id'])==player:
                    selected={'key':player,'name':p['last_name']};break
            if selected is None:raise LookupError('player not found')
        player_rows=db.execute('SELECT player_id,last_name FROM players ORDER BY last_name COLLATE BINARY,player_id LIMIT 101 OFFSET ?',
                               (players_page*100,)).fetchall()
    value=None if row is None else row['amount']
    stamp=None if row is None else row['occurred_at_ms']
    types=[] if row is None or page else [{'type':key,'label':selected_label,'value':value}]
    return {'catalog':catalog,'category':'minecraft:custom','key':key,'label':selected_label,'unit':unit,
            'period':{'label':label,'timezone':'JST (UTC+09:00)','start_ms':args['start'],'end_ms_exclusive':args['end']},
            'selected_player':selected,'players':[{'key':analytics.player_key(p['player_id']),'name':p['last_name'],'value':None}
                for p in player_rows[:100]],'players_page':players_page,'players_has_more':len(player_rows)>100,
            'totals':{'value':value,'source_rows':int(row is not None),'first_ms':stamp,'last_ms':stamp},
            'types':types,'type_count':int(row is not None),'page':page,'limit':limit,'has_more':False,
            'ranking':[],'rank_page':rank_page,'ranking_has_more':False,'ranking_supported':False,
            'ranking_scope':'ワールド共有値（プレイヤー順位対象外）','metric_scope':'world',
            'aggregation':'latest_snapshot','projection_supported':True,
            'status':'no_record' if row is None else 'stale_snapshot' if now_ms-stamp>30000 else 'recorded_snapshot',
            'collection_complete':False,'native_runtime_acceptance':'not_attested','sources':[SOURCE],
            'observation_stage':'world_tick_snapshot','final_world_outcome_verified':False,
            'snapshot':None if row is None else {'observed_at_ms':stamp,'run_key':analytics.player_key('world:'+row['session_id']),'tick_id':value},
            'definition':definition,'limitations':'全9分類／77キーの実機受入は未完了。停止後や欠測中はtickを進めません。再起動・別worldを跨ぐ連続性は未証明。',
            'generated_at_ms':now_ms}
