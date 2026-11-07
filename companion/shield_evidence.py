"""Private-file inspector for raw shield observations. Never a metric/acceptance."""
import argparse
import hashlib
import json
import time
from pathlib import Path
from .analytics import snapshot

SOURCE='ll26.60.player_hurt_shield_evidence.v1'
BOOL_FIELDS=('blocking','blocked_flag','previous_blocked','previous_damaged_blocked','invulnerable','dead')
TOP={'hook','phase','input_milli','result_type','damage_cause','returned_milli','returned_present',
     'may_block','invulnerable_to_source','before','after','same_player','same_level','ambiguous',
     'sample_interval_ms','max_tracked_players','rounding','shield_success_verified',
     'shield_blocked_amount_measured','eligible_for_statistics'}

def _object(pairs):
    d={}
    for key,value in pairs:
        if key in d:raise ValueError('duplicate key')
        d[key]=value
    return d

def _constant(_):raise ValueError('nonfinite JSON')
def _int(value,lo,hi):return type(value) is int and lo<=value<=hi
def _fields(value):
    return (isinstance(value,dict) and set(value)==set(BOOL_FIELDS)|{'shield_time','damaged_shield_time','invulnerable_ticks'}
        and all(type(value.get(k)) is bool for k in BOOL_FIELDS)
        and all(_int(value.get(k),-2**63,2**63-1) for k in ('shield_time','damaged_shield_time'))
        and _int(value.get('invulnerable_ticks'),-2**31,2**31-1))

def parse(raw):
    if not isinstance(raw,str) or len(raw)>2048:return None
    try:d=json.loads(raw,object_pairs_hook=_object,parse_constant=_constant)
    except (ValueError,RecursionError):return None
    if not isinstance(d,dict) or set(d)!=TOP:return None
    if (d['hook']!='Player::$_hurt' or d['phase']!='post_origin_raw_fields' or d['rounding']!='nearest_milli'
        or not _int(d['result_type'],0,2) or not _int(d['damage_cause'],-2**31,2**31-1)
        or not all(d[k] is None or _int(d[k],0,100000000) for k in ('input_milli','returned_milli'))
        or not all(type(d[k]) is bool for k in ('returned_present','may_block','invulnerable_to_source','ambiguous'))
        or (not d['returned_present'] and d['returned_milli'] is not None)
        or not _fields(d['before']) or not _fields(d['after'])
        or not all(d[k] is True for k in ('same_player','same_level'))
        or not _int(d['sample_interval_ms'],5000,5000) or not _int(d['max_tracked_players'],256,256)
        or not all(d[k] is False for k in ('shield_success_verified','shield_blocked_amount_measured','eligible_for_statistics'))):return None
    return d

def inspect(database,*,limit=100,now_ms=None):
    if not _int(limit,1,1000):raise ValueError('limit must be1..1000')
    now_ms=time.time_ns()//1000000 if now_ms is None else now_ms
    with snapshot(Path(database)) as db:
        columns={r['name'] for r in db.execute('PRAGMA table_info(events)')}
        session='session_id' if 'session_id' in columns else 'NULL AS session_id'
        rows=db.execute('SELECT event_id,occurred_at_ms,actor_id,actor_name,details_json,'+session+''' FROM events
            WHERE event_kind='player_shield_evidence' AND cause=? AND actor_kind='player'
            AND typeof(actor_id)='text' AND length(actor_id) BETWEEN 1 AND 128 AND typeof(actor_name)='text'
            AND primary_type='minecraft:shield_evidence' AND secondary_type=''
            AND typeof(amount)='integer' AND amount=0 AND typeof(occurred_at_ms)='integer'
            AND occurred_at_ms>=0 AND occurred_at_ms<=? ORDER BY event_id DESC LIMIT ?''',(SOURCE,now_ms,limit+1)).fetchall()
    entries=[];invalid=0
    for row in rows[:limit]:
        data=parse(row['details_json'])
        if data is None or (row['session_id'] is not None and not isinstance(row['session_id'],str)):
            invalid+=1;continue
        key=lambda value:hashlib.sha256(value.encode('utf-8')).hexdigest()[:32]
        entries.append({'event_id':row['event_id'],'at_ms':row['occurred_at_ms'],
            'player_key':key(row['actor_id']),'player_name':row['actor_name'],
            'activation_key':None if row['session_id'] is None else key(row['session_id']),'raw_observation':data})
    return {'kind':'shield-raw-evidence-inspection-v1','source':SOURCE,'entries':entries,
        'scanned_subset':min(len(rows),limit),'invalid_in_subset':invalid,'has_more':len(rows)>limit,
        'shield_metric_available':False,'full_parity_accepted':False,
        'definition':'型付きフィールドの前後観測のみ。盾成功・実軽減量ではなく、統計へ加算しません。'}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database',type=Path,required=True);parser.add_argument('--limit',type=int,default=100)
    args=parser.parse_args()
    print(json.dumps(inspect(args.database,limit=args.limit),ensure_ascii=False,indent=2))

if __name__=='__main__':main()
