"""Checker fixtures only. No BDS or claim of real player action acceptance."""
import copy
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import unittest

from companion.action_acceptance import (METRICS,SOURCES,DROP_SOURCE,DEATH_SOURCE,MODE_SOURCE,PICKUP_SOURCE,
    JUMP_SOURCE,KILL_SOURCE,USE_SOURCE,HURT_SOURCE,BUSY_START,BUSY_END,action_totals,check_expected,check_run,
    expected_metrics,check_api,verify_http_dom,owned_file,PROJECT,check_session,action_periods)
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW

ACTOR='uuid64:1:2'
ACTIVATION='lltick-1-2-1'
DROP={'hook':'Player::$drop','phase':'return_true','randomly':False,'quantity':'entry_stack_count',
      'inventory_delta_verified':False}


def row(event_id=2,source=DROP_SOURCE,**patch):
    r={'event_id':event_id,'occurred_at_ms':BASE+1000,'event_kind':SOURCES[source],
       'actor_kind':'player','actor_id':ACTOR,'amount':7,'primary_type':'minecraft:stone',
       'secondary_type':'','cause':source,'session_id':ACTIVATION,'details_json':json.dumps(DROP)}
    r.update(patch);return r


def markers(actions):
    begin=row(1,event_kind='server_tick',actor_kind='system',cause=BUSY_START,occurred_at_ms=BASE-1)
    end=row(999,event_kind='server_tick',actor_kind='system',cause=BUSY_END,occurred_at_ms=BASE+10000)
    return [begin,*actions,end]


def run_evidence():
    access={'mode':'one-real-player','player_name':'Example','player_xuid':'1'}
    capture={key:True for key in ('event_to_db_verified','database_unchanged','sqlite_integrity_ok','schema_version_ok',
        'real_client_connect_log','real_client_disconnect_log','normal_disable_log','normal_quit_log')}
    capture.update(foreign_key_issues=0,capture_access=access,expected_player_name='Example',expected_player_xuid='1',
        candidate_dll_sha256='A'*64,sandbox_root='fixture-only-not-real-root',
        player_rows={'database_pair_verified':True,'issues':[],'actor_id':ACTOR,'join_rows':1,'leave_rows':1,
            'session_id':'native-session','session':{'player_id':ACTOR,'session_id':'native-session',
                'started_at_ms':BASE,'ended_at_ms':BASE+10000,'duration_ms':10000}})
    runtime={key:True for key in ('native_loaded','native_enabled','server_started','stop_command_issued',
        'bds_load_verified','loader_config_migrated_with_disabled_overrides')}
    runtime.update(exit_code=0,forced_termination=False,error=None,capture_access=access,
        candidate_dll_sha256='A'*64,sandbox_root=capture['sandbox_root'],player_window_seconds=600,
        startup_outcome={'stop_reason':'player_roundtrip_observed'},
        preflight={'candidate_policy_passed':True,'capture_access':access},
        started_utc='2026-11-05T14:59:59Z',finished_utc='2026-11-05T15:00:20Z')
    return capture,runtime


class ActionEvidenceTests(unittest.TestCase):
    def test_required_actual_gates_normal_stop_identity_windows(self):
        c,r=run_evidence();self.assertEqual(check_run(c,r),(ACTOR,BASE,BASE+10000))
        for value in (30,300,600,3600):
            x=copy.deepcopy(r);x['player_window_seconds']=value;check_run(c,x)
        for value in (None,True,29,3601,600.0,'600'):
            x=copy.deepcopy(r);x['player_window_seconds']=value
            with self.subTest(window=value),self.assertRaises(ValueError):check_run(c,x)
        mutations=[('forced_termination',True),('exit_code',-1),('exit_code',False),('error','crash'),
                   ('native_enabled',False),('native_loaded',1),('candidate_dll_sha256','B'*64),
                   ('sandbox_root','other'),('startup_outcome',{'stop_reason':'ready_hold_completed'}),
                   ('capture_access',{'mode':'empty-smoke'})]
        for key,value in mutations:
            x=copy.deepcopy(r);x[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):check_run(c,x)
        for key in ('event_to_db_verified','database_unchanged','normal_quit_log','sqlite_integrity_ok'):
            x=copy.deepcopy(c);x[key]=False
            with self.subTest(key=key),self.assertRaises(ValueError):check_run(x,r)
        x=copy.deepcopy(c);x['player_rows']['session']['duration_ms']=999
        with self.assertRaises(ValueError):check_run(x,r)

    def test_operator_expectations_are_positive_supported_and_explicit(self):
        def obs(expected,**patch):return dict(kind='operator-action-observation-v1',observed_in_game=True,expected=expected)|patch
        self.assertEqual(expected_metrics(obs({'drop_actions':2})),{'drop_actions':2})
        for value in (obs({}),obs({'craft':1}),obs({'drop_actions':0}),obs({'drop_actions':True}),
                      obs({'drop_actions':1000001}),obs({'drop_actions':1},observed_in_game=False),{},
                      obs({'drop_actions':1.0}),obs({'drop_actions':1},kind='fixture')):
            with self.assertRaises(ValueError):expected_metrics(value)

    def test_drop_rows_not_stack64_fixture_or_attempt_and_activation_order(self):
        actions=[row(),row(3,amount=2)]
        totals,sources=action_totals(markers(actions),ACTOR,BASE,BASE+10000)
        self.assertEqual(totals['drop_actions'],2);self.assertEqual(totals['dropped_items'],9)
        self.assertEqual(sources,{DROP_SOURCE});check_expected({'dropped_items':9},totals)
        with self.assertRaises(ValueError):check_expected({'dropped_items':8},totals)
        bads=[row(amount=0),row(amount=256),row(amount=True),row(event_kind='item_use_attempt'),
              row(actor_id='uuid64:9:9'),row(session_id=''),row(occurred_at_ms=BASE-1),
              row(occurred_at_ms=BASE+10001),row(primary_type='minecraft:<svg>'),
              row(details_json=json.dumps(DROP|{'randomly':True})),row(details_json='['*600)]
        for action in bads:
            with self.subTest(action=action),self.assertRaises(ValueError):action_totals(markers([action]),ACTOR,BASE,BASE+10000)
        for rows in ([row()],[row(),row()],markers([row(),row(3,session_id='lltick-2-2-1')]),
                     markers([row()])+[row(1000,event_kind='server_tick',actor_kind='system',cause=BUSY_END)]):
            with self.assertRaises(ValueError):action_totals(rows,ACTOR,BASE,BASE+10000)
        ignored=row(cause='ll26.60.item_use_attempt.v1',event_kind='item_use_attempt')
        totals,sources=action_totals([ignored],ACTOR,BASE,BASE+10000)
        self.assertTrue(all(x==0 for x in totals.values()));self.assertEqual(sources,set())

    def test_all_existing_source_arithmetic_and_bad_details(self):
        pickup={'hook':'Player::take','phase':'return_true_inventory_delta','scope':'main_inventory_type_total',
            'entry_units':5,'slots':36,'before_units':10,'after_units':13,'nested':False,'durable_transfer_verified':False}
        use={'hook':'GameMode::$useItem','phase':'return_true_inventory_snapshot','scope':'main_inventory_type_total',
            'entry_units':16,'slots':36,'before_units':16,'after_units':15,'loss_units':1,'nested':False,
            'effect_completed_verified':False,'consumption_verified':False}
        death={'hook':'Player::$die','phase':'post_dead_transition','before_dead':False,'after_dead':True,
            'killer_attribution':'unmeasured'}
        jump={'hook':'Mob::jumpFromGround','phase':'post_upward_impulse','measurement':'pos_delta_y_x1e6',
            'grounded_before':True,'vehicle_before':False,'vehicle_after':False,'before_y_micro':0,
            'after_y_micro':420000,'dimension':0,'world_jump_verified':False}
        kill={'hook':'Mob::$die','phase':'post_dead_transition','damage_cause':2,'source_entity':True,
            'source_child':False,'source_block':False,'source_ids_equal':True,'resolved_player':True,
            'self':False,'before_dead':False,'after_dead':True,'nested':False,'world_death_verified':False}
        mode={'phase':'delayed_flag_observation','coalesced':True,'duration_measured':False,'poll_us':250000,
            'flush_us':5000000,'first_ms':BASE+500,'last_ms':BASE+1000,'jst_day':(BASE+32400000)//86400000}
        actions=[row(2,DEATH_SOURCE,amount=1,primary_type='minecraft:player',secondary_type='cause:5',details_json=json.dumps(death)),
            row(3,PICKUP_SOURCE,amount=3,details_json=json.dumps(pickup)),
            row(4,USE_SOURCE,amount=1,secondary_type='main_hand',details_json=json.dumps(use)),
            row(5,JUMP_SOURCE,amount=1,primary_type='minecraft:player',details_json=json.dumps(jump)),
            row(6,KILL_SOURCE,amount=1,primary_type='minecraft:zombie',secondary_type='mob',details_json=json.dumps(kill)),
            row(7,KILL_SOURCE,amount=1,primary_type='minecraft:player',secondary_type='player',details_json=json.dumps(kill))]
        for key in ('sprint','sneak'):
            for edge in ('start','stop'):
                actions.append(row(len(actions)+2,MODE_SOURCE,amount=2,primary_type=key,secondary_type=edge,details_json=json.dumps(mode)))
        totals,sources=action_totals(markers(actions),ACTOR,BASE,BASE+10000)
        self.assertEqual(totals['direct_kills'],2);self.assertEqual(totals['direct_player_kills'],1)
        self.assertEqual(totals['confirmed_deaths'],1);self.assertEqual(totals['pickup_inventory_items'],3)
        self.assertEqual(totals['use_inventory_loss'],1);self.assertEqual(totals['jump_impulses'],1)
        self.assertTrue(all(totals[k]==2 for k in ('sprint_starts','sprint_stops','sneak_starts','sneak_stops')))
        self.assertEqual(sources,set(SOURCES)-{DROP_SOURCE,HURT_SOURCE})
        for action in actions:
            bad=action|{'details_json':'{}'}
            with self.subTest(source=action['cause']),self.assertRaises(ValueError):action_totals(markers([bad]),ACTOR,BASE,BASE+10000)
        invalid_mode=actions[-1]|{'details_json':json.dumps(mode|{'first_ms':BASE-1})}
        with self.assertRaises(ValueError):action_totals(markers([invalid_mode]),ACTOR,BASE,BASE+10000)

    def test_owned_path_rejects_protected_outside_and_oversize(self):
        base=PROJECT/'.build'/'real-player-evidence'
        with self.assertRaises(ValueError):owned_file(PROJECT/'STATUS.md',base,10000000)
        with self.assertRaises(ValueError):owned_file(PROJECT/'STATUS.md',PROJECT,1)
        with self.assertRaises(ValueError):owned_file(PROJECT/'../other.sqlite',PROJECT,100)

    def test_jst_month_boundary_plans_use_raw_counts_not_all_time(self):
        from datetime import datetime,timezone
        boundary=int(datetime(2026,10,31,15,tzinfo=timezone.utc).timestamp()*1000)
        rows=markers([row(2,occurred_at_ms=boundary-1,amount=2),row(3,occurred_at_ms=boundary,amount=7)])
        plans=action_periods(rows,ACTOR,boundary-1000,boundary+1000)
        values={(kind,date):t['dropped_items'] for kind,date,t in plans}
        self.assertEqual(values,{('all',None):9,('day','2026-10-31'):2,('day','2026-11-01'):7,
                                 ('month','2026-10'):2,('month','2026-11'):7})


class ActionHttpEvidenceTests(Fixture):
    def setUp(self):
        super().setUp();self.player(ACTOR,'Example')
        with fixture_writer(self.path) as db:
            db.execute('ALTER TABLE events ADD COLUMN session_id TEXT')
        self.session('native-session',BASE,BASE+10000,ACTOR)
        with fixture_writer(self.path) as db:
            for kind,stamp in (('session_start',BASE),('session_end',BASE+10000)):
                db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,actor_name,cause,session_id)
                    VALUES(?,?,'player',?,'Example','levilamina_player_event','native-session')''',(stamp,kind,ACTOR))
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json,session_id) VALUES(?,'item_drop_result','player',?,?,7,
                'minecraft:stone','',?,?)''',(BASE+1000,ACTOR,DROP_SOURCE,json.dumps(DROP),ACTIVATION))

    def test_independent_rows_http401200_selected_rank_actual_assets_dom_and_no_writes(self):
        c,_=run_evidence()
        db=sqlite3.connect(self.path)
        try:check_session(db,c,ACTOR,BASE,BASE+10000)
        finally:db.close()
        before=hashlib.sha256(self.path.read_bytes()).digest()
        totals,_=action_totals(markers([row()]),ACTOR,BASE,BASE+10000)
        proof=verify_http_dom(self.path,ACTOR,{'drop_actions':1,'dropped_items':7},totals)
        self.assertEqual(proof['anonymous'],401);self.assertEqual(proof['authenticated'],200)
        self.assertEqual(proof['ui_dom']['metrics']['dropped_items'],'7 個')
        self.assertTrue(proof['listener_stopped_only_own']);self.assertFalse(proof['browser_verified'])
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)
        self.assertNotIn('Example',json.dumps(proof));self.assertNotIn(ACTOR,json.dumps(proof))
        self.assertNotIn('Bearer',json.dumps(proof))

    def test_actual_http_dom_verifies_jst_day_month_and_all(self):
        rows=markers([row()]);totals,_=action_totals(rows,ACTOR,BASE,BASE+10000)
        plans=action_periods(rows,ACTOR,BASE,BASE+10000)
        proof=verify_http_dom(self.path,ACTOR,{'drop_actions':1,'dropped_items':7},totals,plans)
        self.assertEqual([p['kind'] for p in proof['period_checks']],['all','day','month'])
        self.assertEqual([p['date'] for p in proof['period_checks']],[None,'2026-11-06','2026-11'])
        self.assertTrue(all(p['metrics']['dropped_items']==7 for p in proof['period_checks']))

    def test_api_or_raw_session_tamper_never_passes(self):
        totals,_=action_totals(markers([row()]),ACTOR,BASE,BASE+10000)
        expected={'drop_actions':1}
        data=self.report(period='all',metric='drop_actions',player=self.analytics.player_key(ACTOR))
        check_api(data,expected,totals)
        bad=copy.deepcopy(data);bad['selected_player']['dropped_items']=64
        with self.assertRaises(ValueError):check_api(bad,expected,totals)
        bad=copy.deepcopy(data);bad['ranking'][0]['value']=2
        with self.assertRaises(ValueError):check_api(bad,expected,totals)
        c,_=run_evidence()
        with fixture_writer(self.path) as db:db.execute('UPDATE sessions SET duration_ms=9999')
        db=sqlite3.connect(self.path)
        try:
            with self.assertRaises(ValueError):check_session(db,c,ACTOR,BASE,BASE+10000)
        finally:db.close()

    def test_cli_rejects_a_fixture_as_host_evidence_without_emitting_pass(self):
        done=subprocess.run([sys.executable,'-m','companion.action_acceptance','--capture',str(self.path),
            '--observations',str(self.path)],cwd=PROJECT,capture_output=True,text=True,timeout=5)
        self.assertNotEqual(done.returncode,0);self.assertEqual(done.stdout,'')
        self.assertIn('Evidence outside the permitted project area',done.stderr)
