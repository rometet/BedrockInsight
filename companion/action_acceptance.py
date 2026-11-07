"""Consistency check for an externally executed, normally stopped player trial.

Never launches/controls BDS, reads its original DB, inserts events or restarts
18767. CLI accepts only the private closed copy produced by the existing host
verifier. Operator observations are required, not generated from SQL/API data.
Evidence consistency is NOT independent attestation of gameplay or edited files.
"""
import argparse
from datetime import datetime
import hashlib
import http.client
import json
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess
import threading
import time

from .analytics import JST,period_bounds,valid_item_type
from .death_source import DEATH_SOURCE,cause_number,valid_death_details
from .jump_source import JUMP_SOURCE,JUMP_EVENT_SOURCE,valid_jump_details
from .kill_source import KILL_SOURCE,valid_kill_details
from .mode_source import MODE_SOURCE,valid_mode_details
from .pickup_source import PICKUP_SOURCE,valid_pickup_details
from .use_source import USE_SOURCE,use_loss
from .hurt_source import HURT_SOURCE,valid_hurt_details
from .mspt_acceptance import require,utc_ms
from .telemetry import BUSY_START,BUSY_END
from .server import CompanionServer

PROJECT=Path(__file__).resolve().parents[1]
DROP_SOURCE='ll26.60.player_drop_returned_true.v1'
SOURCES={DROP_SOURCE:'item_drop_result',DEATH_SOURCE:'player_death',MODE_SOURCE:'player_mode_observation',
         PICKUP_SOURCE:'item_pickup_delta',JUMP_SOURCE:'player_jump_impulse',
         JUMP_EVENT_SOURCE:'player_jump_impulse',
         KILL_SOURCE:'direct_kill_observation',USE_SOURCE:'item_use_result',HURT_SOURCE:'item_hurt_slot_loss'}
METRICS=('dropped_items','drop_actions','confirmed_deaths','sprint_starts','sprint_stops',
         'sneak_starts','sneak_stops','pickup_inventory_items','pickup_delta_actions','jump_impulses',
         'direct_kills','direct_mob_kills','direct_player_kills','use_return_actions','use_inventory_loss','hurt_slot_losses')
MAX_BYTES=256*1024*1024


def digest(path):
    value=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(256*1024),b''):
            value.update(block)
    return value.hexdigest().upper()


def owned_file(path,base,limit):
    path=Path(path).absolute()
    require('..' not in path.parts and path.is_relative_to(base),'Evidence outside the permitted project area')
    for ancestor in (path,*path.parents):
        require(not ancestor.is_symlink() and not getattr(ancestor,'is_junction',lambda:False)(),
                'Redirected evidence path refused')
        if ancestor==PROJECT:break
    path=path.resolve(strict=True)
    require(path.is_relative_to(base) and path.is_file() and path.stat().st_size<=limit,
            'Evidence is not a bounded owned file')
    return path


def read_json(path):
    value=json.loads(path.read_text(encoding='utf-8-sig'))
    require(isinstance(value,dict),'Evidence JSON must be an object')
    return value


def expected_metrics(observation):
    require(observation.get('kind')=='operator-action-observation-v1' and
            observation.get('observed_in_game') is True,'Actual operator observations must be supplied')
    expected=observation.get('expected')
    require(isinstance(expected,dict) and 1<=len(expected)<=len(METRICS),'Need at least one observed action metric')
    for key,value in expected.items():
        require(key in METRICS and type(value) is int and 1<=value<=1000000,
                'Only supported positive integer observation metrics are accepted')
    return expected


def check_run(capture,runtime):
    for flag in ('event_to_db_verified','database_unchanged','sqlite_integrity_ok','schema_version_ok',
                 'real_client_connect_log','real_client_disconnect_log','normal_disable_log','normal_quit_log'):
        require(capture.get(flag) is True,'Missing real-client/closed-copy proof: '+flag)
    require(type(capture.get('foreign_key_issues')) is int and capture['foreign_key_issues']==0,
            'Foreign-key proof missing')
    pair=capture.get('player_rows',{})
    require(pair.get('database_pair_verified') is True and pair.get('issues')==[],
            'Missing exact native join/leave session pair')
    require(type(pair.get('join_rows')) is int and pair['join_rows']==1 and
            type(pair.get('leave_rows')) is int and pair['leave_rows']==1,'Not one real-client session')
    actor=pair.get('actor_id')
    require(isinstance(actor,str) and re.fullmatch(r'uuid64:[0-9]{1,20}:[0-9]{1,20}',actor),
            'Missing native player identity')
    words=[int(x) for x in actor[7:].split(':')]
    require(all(0<=x<2**64 for x in words) and any(words),'Invalid native player identity')
    for flag in ('native_loaded','native_enabled','server_started','stop_command_issued','bds_load_verified',
                 'loader_config_migrated_with_disabled_overrides'):
        require(runtime.get(flag) is True,'Missing actual runtime gate: '+flag)
    require(type(runtime.get('exit_code')) is int and runtime['exit_code']==0 and
            runtime.get('forced_termination') is False and runtime.get('error') is None,
            'Need graceful exit0 without forced termination/error')
    require(runtime.get('startup_outcome',{}).get('stop_reason')=='player_roundtrip_observed' and
            type(runtime.get('player_window_seconds')) is int and 30<=runtime['player_window_seconds']<=3600,
            'Need bounded actual player roundtrip, not empty smoke')
    require(runtime.get('preflight',{}).get('candidate_policy_passed') is True,'Missing isolation preflight')
    for access in (capture.get('capture_access',{}),runtime.get('capture_access',{}),
                   runtime.get('preflight',{}).get('capture_access',{})):
        require(access.get('mode')=='one-real-player' and access==capture.get('capture_access') and
                access.get('player_name')==capture.get('expected_player_name') and
                access.get('player_xuid')==capture.get('expected_player_xuid'),
                'Not the same explicitly authorized one-real-player policy')
    require(capture.get('sandbox_root')==runtime.get('sandbox_root'),'Mixed isolated roots')
    require(isinstance(capture.get('candidate_dll_sha256'),str) and
            re.fullmatch(r'[A-Fa-f0-9]{64}',capture['candidate_dll_sha256']) and
            capture['candidate_dll_sha256'].upper()==str(runtime.get('candidate_dll_sha256')).upper(),
            'Mixed candidate identities')
    start,end=utc_ms(runtime['started_utc']),utc_ms(runtime['finished_utc'])
    require(0<end-start<=(runtime['player_window_seconds']+360)*1000,'Runtime exceeds the bounded trial window')
    session=pair.get('session',{})
    b,e=session.get('started_at_ms'),session.get('ended_at_ms')
    require(all(type(x) is int for x in (b,e,session.get('duration_ms'))) and
            0<=start<=b<e<=end and session['duration_ms']==e-b and
            session.get('player_id')==actor and session.get('session_id')==pair.get('session_id'),
            'Invalid closed player-session window')
    return actor,b,e


def action_totals(rows,actor,start,end):
    """Independent raw-row arithmetic. Invalid exact-source rows fail closed."""
    require(len(rows)<=1000,'Action row budget exceeded')
    totals={key:0 for key in METRICS};activations=set();previous=0;used_sources=set();action_ids=[]
    for row in rows:
        event_id=row['event_id']
        require(type(event_id) is int and event_id>previous,'Duplicate/unordered durable events')
        previous=event_id
        source=row['cause']
        if source not in SOURCES:continue # legacy/attempts/unsupported never accepted as success
        require(row['event_kind']==SOURCES[source] and row['actor_kind']=='player' and row['actor_id']==actor,
                'Action source/kind/actor mismatch')
        stamp,amount=row['occurred_at_ms'],row['amount']
        require(type(stamp) is int and start<=stamp<=end,'Action outside the real player session')
        require(type(amount) is int and 1<=amount<=1000000,'Invalid action amount')
        activation=row['session_id']
        require(isinstance(activation,str) and re.fullmatch(r'lltick-[A-Za-z0-9-]{1,120}',activation),
                'Missing bounded native activation identity')
        activations.add(activation);used_sources.add(source)
        action_ids.append(event_id)
        require(len(activations)==1,'Mixed activations are not a single trial')
        item,secondary,raw=row['primary_type'],row['secondary_type'],row['details_json']
        if source==MODE_SOURCE:
            require(valid_mode_details(raw,item,secondary,stamp,amount)==1,'Invalid delayed-mode details')
            d=json.loads(raw)
            require(start<=d['first_ms']<=stamp,'Mode batch extends outside the real session')
            totals[('sprint_' if item=='sprint' else 'sneak_')+('starts' if secondary=='start' else 'stops')]+=amount
        elif source==DROP_SOURCE:
            require(valid_item_type(item) and amount<=255 and secondary in ('',None) and
                    isinstance(raw,str) and len(raw)<=512,'Invalid drop scope/amount')
            try:d=json.loads(raw)
            except (ValueError,RecursionError):raise ValueError('Invalid drop JSON') from None
            require(isinstance(d,dict) and d.get('hook')=='Player::$drop' and d.get('phase')=='return_true' and
                    d.get('randomly') is False and d.get('quantity')=='entry_stack_count' and
                    d.get('inventory_delta_verified') is False,'Drop is not a true nonrandom entry-stack observation')
            totals['drop_actions']+=1;totals['dropped_items']+=amount
        elif source==PICKUP_SOURCE:
            require(valid_item_type(item) and secondary in ('',None) and valid_pickup_details(raw,amount)==1,
                    'Invalid pickup delta')
            totals['pickup_delta_actions']+=1;totals['pickup_inventory_items']+=amount
        elif source==USE_SOURCE:
            loss=use_loss(raw)
            require(valid_item_type(item) and amount==1 and secondary in ('main_hand','off_hand') and
                    loss is not None,'Invalid use response/inventory snapshot')
            totals['use_return_actions']+=1;totals['use_inventory_loss']+=loss
        elif source==HURT_SOURCE:
            require(amount==1 and valid_item_type(item) and secondary in ('',None) and
                    valid_hurt_details(raw)==1,'Invalid hurt slot observation')
            totals['hurt_slot_losses']+=1
        elif source==KILL_SOURCE:
            require(amount==1 and valid_item_type(item) and valid_kill_details(raw)==1 and
                    ((secondary=='player' and item=='minecraft:player') or
                     (secondary=='mob' and item!='minecraft:player')),'Invalid direct-source death observation')
            totals['direct_kills']+=1;totals['direct_'+secondary+'_kills']+=1
        else:
            require(amount==1 and item=='minecraft:player','Invalid player observation')
            if source==DEATH_SOURCE:
                require(cause_number(secondary) is not None and valid_death_details(raw)==1,'Invalid dead transition')
                totals['confirmed_deaths']+=1
            else:
                require(secondary in ('',None) and valid_jump_details(raw)==1,'Invalid jump impulse')
                totals['jump_impulses']+=1
    if action_ids:
        activation=next(iter(activations))
        for cause,before in ((BUSY_START,True),(BUSY_END,False)):
            markers=[r for r in rows if r['cause']==cause]
            require(len(markers)==1 and markers[0]['event_kind']=='server_tick' and
                    markers[0]['actor_kind']=='system' and markers[0]['session_id']==activation,
                    'Missing unique normal activation marker')
            marker_id=markers[0]['event_id']
            require(marker_id<min(action_ids) if before else marker_id>max(action_ids),
                    'Action outside durable activation markers')
    return totals,used_sources


def check_expected(expected,totals):
    for metric,value in expected.items():
        require(totals[metric]==value,'Operator/raw-row mismatch: '+metric)


def check_session(db,capture,actor,start,end):
    require(db.execute('SELECT COUNT(*) FROM players').fetchone()[0]==1 and
            db.execute('SELECT COUNT(*) FROM sessions').fetchone()[0]==1,
            'Checker requires the fresh single-participant trial, not historical/PvP data')
    player=db.execute('SELECT last_name FROM players WHERE player_id=?',(actor,)).fetchone()
    require(player is not None and player[0]==capture.get('expected_player_name'),'Snapshot player mapping mismatch')
    row=db.execute('SELECT session_id,started_at_ms,ended_at_ms,duration_ms FROM sessions WHERE player_id=?',
                   (actor,)).fetchone()
    sid=capture['player_rows']['session_id']
    require(row is not None and tuple(row)==(sid,start,end,end-start),'Snapshot/closed session mismatch')
    pair=db.execute('''SELECT event_kind,occurred_at_ms,actor_kind,session_id,cause FROM events
        WHERE actor_id=? AND event_kind IN ('session_start','session_end') ORDER BY event_id''',(actor,)).fetchall()
    require([tuple(x) for x in pair]==[
        ('session_start',start,'player',sid,'levilamina_player_event'),
        ('session_end',end,'player',sid,'levilamina_player_event')],'Snapshot/native session events mismatch')


def action_periods(rows,actor,start,end):
    totals,_=action_totals(rows,actor,start,end)
    days={datetime.fromtimestamp(r['occurred_at_ms']//1000,JST).strftime('%Y-%m-%d')
          for r in rows if r['cause'] in SOURCES}
    require(len(days)<=2,'A bounded trial cannot span more than two JST dates')
    plans=[('all',None,totals)]
    for kind,labels in (('day',days),('month',{day[:7] for day in days})):
        for label in sorted(labels):
            b,e,_=period_bounds(kind,label,max(end,int(time.time()*1000)))
            scoped=[r for r in rows if r['cause'] not in SOURCES or b<=r['occurred_at_ms']<e]
            plans.append((kind,label,action_totals(scoped,actor,start,end)[0]))
    return plans


def check_api(data,expected,totals,period='all'):
    require(data.get('companion') is True and data.get('read_only') is True and
            data.get('period',{}).get('kind')==period and
            data.get('period',{}).get('timezone')=='JST (UTC+09:00)','Not the read-only requested JST companion period')
    player=data.get('selected_player')
    require(isinstance(player,dict),'Missing selected player in authenticated API')
    for metric in METRICS:
        value=player.get(metric)
        require((value is None if totals[metric]==0 else type(value) is int and value==totals[metric]),
                'Raw rows/API mismatch: '+metric)
    require(data.get('metric') in expected,'Unexpected API ranking metric')
    matches=[r for r in data.get('ranking',[]) if r.get('key')==player.get('key')]
    value=totals[data['metric']]
    require((len(matches)==1 and type(matches[0].get('value')) is int and matches[0]['value']==value)
            if value else matches==[],'Selected player/ranking mismatch')


def verify_http_dom(path,actor,expected,totals,periods=None):
    node=shutil.which('node');require(node is not None,'Node is required for the actual-assets DOM check')
    server=CompanionServer(path,0)
    thread=threading.Thread(target=server.serve_forever,kwargs={'poll_interval':0.01})
    try:thread.start()
    except BaseException:
        server.server_close();raise
    def get(url,auth=False):
        client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
        try:
            client.request('GET',url,headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
            response=client.getresponse();body=response.read(2*1024*1024+1)
            require(len(body)<=2*1024*1024,'API response exceeds budget')
            return response.status,body
        finally:client.close()
    try:
        require(get('/api/v1/analytics?period=all')[0]==401,'Anonymous API must reject401')
        assets={}
        for url,name in (('/actions','actions.html'),('/actions.js','actions.js')):
            code,body=get(url);require(code==200,'Missing actual action assets')
            assets[name]=hashlib.sha256(body).hexdigest().upper()
            require(assets[name]==digest(PROJECT/'companion'/'web'/name),'HTTP and evaluated assets differ')
        selected=server.analytics.player_key(actor)
        period_checks=[]
        for kind,label,counts in (periods or [('all',None,totals)]):
            for metric in expected:
                url='/api/v1/analytics?period='+kind+'&limit=100&metric='+metric+'&player='+selected
                if label:url+='&date='+label
                code,body=get(url,True)
                require(code==200,'Authenticated analytics must return200')
                data=json.loads(body);check_api(data,expected,counts,kind)
                require(data['selected_player']['key']==selected and (not label or data['period']['label']==label),
                        'API selected another player/date')
            scoped={key:(counts[key] or None) for key in expected}
            completed=subprocess.run([node,str(PROJECT/'tests/native_actions_ui_check.cjs')],
                input=json.dumps({'data':data,'expected':scoped}),encoding='utf-8',
                capture_output=True,timeout=8)
            # Never surface raw report names/tokens or subprocess diagnostics.
            require(completed.returncode==0,'Japanese action DOM consistency failed')
            rendered=json.loads(completed.stdout)
            require(rendered.get('verification')=='actual Japanese /actions assets DOM, not browser' and
                    set(rendered.get('metrics',{}))==set(expected),'Missing honest action DOM proof')
            period_checks.append({'kind':kind,'date':label,'metrics':scoped,'ui_dom':rendered})
        dom=period_checks[0]['ui_dom']
        require(all(value==digest(PROJECT/'companion'/'web'/name) for name,value in assets.items()),
                'Action assets changed during verification')
    finally:
        server.shutdown();server.server_close();thread.join(3)
    require(not thread.is_alive(),'Own ephemeral HTTP thread did not stop')
    return {'anonymous':401,'authenticated':200,'ranking_metrics':list(expected),'ui_dom':dom,
            'period_checks':period_checks,'assets_sha256':assets,'listener_stopped_only_own':True,'browser_verified':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True,help='Host real-player-evidence.json (private)')
    parser.add_argument('--observations',type=Path,required=True,help='Operator action observation JSON, NOT derived counts')
    args=parser.parse_args()
    capture_path=owned_file(args.capture,PROJECT/'.build'/'real-player-evidence',1024*1024)
    require(capture_path.name=='real-player-evidence.json','Need the original host verifier report')
    capture_hash=digest(capture_path)
    capture=read_json(capture_path)
    root=Path(capture['sandbox_root'])
    require(root.name!='run-2bbb60dddd2344c3923ae1cbd66fbb98','Protected original player world is out of scope')
    runtime_path=owned_file(root/'capture-runtime-evidence.json',PROJECT/'.build'/'isolated-bds',1024*1024)
    staging_path=owned_file(root/'capture-staging-evidence.json',PROJECT/'.build'/'isolated-bds',1024*1024)
    require(digest(runtime_path)==str(capture.get('runtime_evidence_sha256')).upper() and
            digest(staging_path)==str(capture.get('staging_evidence_sha256')).upper(),'Host evidence digest mismatch')
    runtime=read_json(runtime_path);actor,start,end=check_run(capture,runtime)
    stdout_path=owned_file(root/'capture-stdout.log',PROJECT/'.build'/'isolated-bds',64*1024*1024)
    require(digest(stdout_path)==str(capture.get('stdout_sha256')).upper(),'Host stdout digest mismatch')
    dll=owned_file(root/'mods'/'BedrockInsight'/'BedrockInsight.dll',PROJECT/'.build'/'isolated-bds',MAX_BYTES)
    require(digest(dll)==capture['candidate_dll_sha256'].upper(),'Loaded candidate digest mismatch')
    path=owned_file(capture['database_snapshot'],capture_path.parent,MAX_BYTES)
    require(path.name=='closed_snapshot.sqlite3' and
            not any(Path(str(path)+s).exists() for s in ('-wal','-shm','-journal')),'Need the private CLOSED snapshot, not a live DB')
    before=digest(path);require(before==str(capture.get('database_sha256')).upper(),'Closed snapshot digest mismatch')
    observations_path=owned_file(args.observations,PROJECT,65536)
    observations_hash=digest(observations_path)
    expected=expected_metrics(read_json(observations_path))
    db=sqlite3.connect(path.as_uri()+'?mode=ro&immutable=1',uri=True)
    try:
        deadline=time.monotonic()+2;db.set_progress_handler(lambda:int(time.monotonic()>deadline),1000)
        db.execute('PRAGMA query_only=ON');db.row_factory=sqlite3.Row
        require(tuple(db.execute('PRAGMA user_version').fetchone())==(4,),'Need unchanged schema4')
        require([tuple(x) for x in db.execute('PRAGMA integrity_check')]==[('ok',)],'Snapshot integrity failure')
        require(db.execute('PRAGMA foreign_key_check').fetchone() is None,'Snapshot foreign-key failure')
        check_session(db,capture,actor,start,end)
        rows=db.execute('SELECT * FROM events ORDER BY event_id LIMIT 1001').fetchall()
        require(len(rows)<=1000,'Fresh trial row budget exceeded')
        totals,sources=action_totals(rows,actor,start,end);check_expected(expected,totals)
        periods=action_periods(rows,actor,start,end)
    finally:db.close()
    http=verify_http_dom(path,actor,expected,totals,periods)
    require(digest(path)==before,'Snapshot changed during verification')
    require(digest(runtime_path)==capture['runtime_evidence_sha256'].upper() and
            digest(staging_path)==capture['staging_evidence_sha256'].upper() and
            digest(stdout_path)==capture['stdout_sha256'].upper() and
            digest(dll)==capture['candidate_dll_sha256'].upper() and digest(capture_path)==capture_hash and
            digest(observations_path)==observations_hash and
            not any(Path(str(path)+s).exists() for s in ('-wal','-shm','-journal')),
            'Host evidence changed during verification')
    print(json.dumps({'verification':'closed action rows/operator notes/authenticated API/Japanese DOM consistency',
        'matched_metrics':expected,'not_accepted_metrics':[x for x in METRICS if x not in expected],
        'observed_sources':sorted(sources),'http':http,'normal_stop':True,'snapshot_unchanged':True,
        'candidate_dll_sha256':capture['candidate_dll_sha256'],'capture_evidence_sha256':capture_hash,
        'snapshot_sha256':before,'observations_sha256':observations_hash,
        'independent_gameplay_attestation':False,
        'limits':'Consistency only; edited evidence or fixture rows do not prove real engine actions. '
                 'Only explicitly observed matching metrics are accepted; no craft/travel/break or full-success inference.'},ensure_ascii=True))


if __name__=='__main__':
    main()
