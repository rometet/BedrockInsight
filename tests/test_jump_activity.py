import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.jump_source import JUMP_SOURCE,JUMP_EVENT_SOURCE,valid_jump_details
from companion.server import CompanionServer
from companion.stats_catalog import type_label
from test_analytics_companion import Fixture,fixture_writer,BASE


class JumpActivityTests(Fixture):
    def jump(self,pid='private-uuid-a',at=BASE+1000,cause=JUMP_SOURCE,kind='player_jump_impulse',
             amount=1,primary='minecraft:player',secondary='',patch=None):
        d={'hook':'Mob::jumpFromGround','phase':'post_upward_impulse','measurement':'pos_delta_y_x1e6',
           'grounded_before':True,'vehicle_before':False,'vehicle_after':False,
           'before_y_micro':-80000,'after_y_micro':420000,'dimension':0,'world_jump_verified':False}
        if patch:d.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,?,'player',?,?,?,?,?,?)''',
                (at,kind,pid,cause,amount,primary,secondary,json.dumps(d)))

    def test_positive_impulse_counts_scope_and_selected(self):
        self.player();self.player('bob','Bob');self.jump();self.jump(at=BASE+2000);self.jump(pid='bob')
        r=self.report(metric='jump_impulses')
        self.assertEqual(r['totals']['jump_impulses'],3)
        self.assertEqual([x['value'] for x in r['ranking']],[2,1])
        self.assertEqual(r['player_jumps']['count'],3)
        key=self.analytics.player_key('bob');r=self.report(player=key,metric='jump_impulses')
        self.assertEqual(r['player_jumps']['count'],1)
        self.assertEqual(r['player_jumps']['scope'],'選択プレイヤー')
        self.assertEqual(r['selected_player']['jump_impulses'],1)
        self.assertIsNone(r['selected_player']['online']);self.assertNotIn('private-uuid',json.dumps(r))

    def test_upstream_player_jump_event_is_counted(self):
        self.player()
        details={'hook':'LeviLamina::PlayerJumpEvent','phase':'post_auth_input',
                 'grounded_and_ascending_checked_by_emitter':True,
                 'final_world_jump_verified':False}
        self.jump(cause=JUMP_EVENT_SOURCE, patch=details)
        report=self.report(metric='jump_impulses')
        self.assertEqual(report['totals']['jump_impulses'],1)
        self.assertEqual(report['player_jumps']['count'],1)
        self.assertEqual(report['player_jumps']['source'],JUMP_EVENT_SOURCE)
        self.assertEqual(report['player_jumps']['legacy_sources'],[JUMP_SOURCE])
        self.assertIn('post_auth_input',report['player_jumps']['definition'])
        self.assertIn('後続リスナーのキャンセル',report['player_jumps']['definition'])
        self.assertEqual(type_label('minecraft:jump'),'ジャンプ通知（旧加速観測を含む）')
        for broken in (dict(phase='pre_auth_input'),
                       dict(grounded_and_ascending_checked_by_emitter=1),
                       dict(final_world_jump_verified=True)):
            self.assertEqual(valid_jump_details(json.dumps(details|broken)),0)

    def test_invalid_source_flags_quantization_amount_and_absence(self):
        self.player()
        for kw in ({'cause':'ll26.60.jump_pos_delta.v2'},{'kind':'player_jump_notification'},
                   {'amount':0},{'amount':2},{'amount':1.5},{'primary':'minecraft:zombie'},
                   {'pid':'unmapped'},{'secondary':'jumping'},
                   {'patch':{'hook':'packet'}},{'patch':{'phase':'before_origin'}},
                   {'patch':{'grounded_before':False}},{'patch':{'grounded_before':1}},
                   {'patch':{'vehicle_before':0}},{'patch':{'vehicle_after':True}},
                   {'patch':{'world_jump_verified':True}},{'patch':{'before_y_micro':-16000001}},
                   {'patch':{'before_y_micro':420000}},{'patch':{'after_y_micro':0}},
                   {'patch':{'after_y_micro':16000001}},{'patch':{'after_y_micro':420000.5}},
                   {'patch':{'dimension':True}},{'patch':{'dimension':2**31}}):
            self.jump(**kw)
        r=self.report(metric='jump_impulses')
        self.assertIsNone(r['totals']['jump_impulses']);self.assertIsNone(r['player_jumps']['count'])
        self.assertEqual(r['ranking'],[])
        for raw in ('','null','[]','{bad','['*600):self.assertEqual(valid_jump_details(raw),0)

    def test_jst_periods_ties_paging_and_old_schema_readonly(self):
        self.player();self.player('z','Alice');self.jump(at=BASE-1);self.jump();self.jump(pid='z')
        r=self.report(metric='jump_impulses',limit=1)
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='jump_impulses',limit=1,page=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        self.assertEqual(self.report(period='day',date='2026-11-05')['totals']['jump_impulses'],1)
        self.assertEqual(self.report(period='month',date='2026-11')['totals']['jump_impulses'],3)
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:db.execute(f'PRAGMA user_version={version}')
            digest=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['jump_impulses'],3)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),digest)

    def test_observation_bounds_and_no_distance_or_airborne_claim(self):
        self.player();self.jump(at=BASE+100);self.jump(at=BASE+2000,patch={'dimension':2})
        self.jump(at=BASE+999999999) # future cannot be used
        r=self.report()
        self.assertEqual(r['player_jumps']['observed'],{'first_ms':BASE+100,'last_ms':BASE+2000})
        self.assertIn('空中移動完了数/距離でもありません',r['player_jumps']['definition'])
        self.assertIn('空中移動が完了したジャンプ数',r['unmeasured'])

    def test_actual_authenticated_json_to_japanese_dom(self):
        self.player(name='<img src=x onerror=alert(1)>');self.jump()
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type)
                VALUES(?,'item_drop_result','player','private-uuid-a','ll26.60.player_drop_returned_true.v1',64,'minecraft:stone')''',(BASE+1000,))
        server=CompanionServer(self.path,0)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        def get(path,auth=False):
            client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=4)
            try:
                client.request('GET',path,headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                response=client.getresponse();return response.status,response.read()
            finally:client.close()
        try:
            url='/api/v1/analytics?period=all&metric=jump_impulses'
            self.assertEqual(get(url)[0],401)
            status,raw=get(url,True);self.assertEqual(status,200)
            self.assertNotIn(b'private-uuid',raw)
            run=subprocess.run(['node','tests/action_dashboard_ui_tests.cjs'],input=raw.decode(),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode,0,run.stderr);self.assertIn('Action DOM PASS',run.stdout)
        finally:server.shutdown();server.server_close();thread.join(3)
