import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.kill_source import KILL_SOURCE,valid_kill_details
from companion.server import CompanionServer
from test_analytics_companion import Fixture,fixture_writer,BASE


class DirectKillActivityTests(Fixture):
    def kill(self,pid='private-uuid-a',victim='minecraft:zombie',kind='mob',at=BASE+1000,
             cause=KILL_SOURCE,event='direct_kill_observation',amount=1,patch=None):
        d={'hook':'Mob::$die','phase':'post_dead_transition','damage_cause':2,'source_entity':True,
           'source_child':False,'source_block':False,'source_ids_equal':True,'resolved_player':True,
           'self':False,'before_dead':False,'after_dead':True,'nested':False,'world_death_verified':False}
        if patch:d.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,?,'player',?,?,?,?,?,?)''',
                (at,event,pid,cause,amount,victim,kind,json.dumps(d)))

    def test_credited_player_mob_player_types_and_selected(self):
        self.player();self.player('bob','Bob')
        self.kill();self.kill();self.kill(victim='minecraft:cow');self.kill(victim='minecraft:player',kind='player')
        self.kill(pid='bob')
        r=self.report(metric='direct_kills')
        self.assertEqual(r['totals']['direct_kills'],5)
        self.assertEqual(r['totals']['direct_mob_kills'],4);self.assertEqual(r['totals']['direct_player_kills'],1)
        self.assertEqual([x['value'] for x in r['ranking']],[4,1])
        r=self.report(player=self.analytics.player_key('private-uuid-a'),metric='direct_player_kills')
        self.assertEqual(r['direct_kills']['types'],[
            {'type':'minecraft:zombie','kind':'mob','count':2},{'type':'minecraft:cow','kind':'mob','count':1},
            {'type':'minecraft:player','kind':'player','count':1}])
        self.assertEqual(r['selected_player']['direct_player_kills'],1)
        self.assertIsNone(r['selected_player']['confirmed_deaths']) # credit is not a victim-death count
        self.assertIsNone(r['selected_player']['online']);self.assertNotIn('private-uuid',json.dumps(r))

    def test_notification_legacy_projectile_environment_self_unknown_not_counted(self):
        self.player()
        for kw in ({'cause':'legacy'},{'cause':'ll26.60.direct_attack_dead_transition.v2'},
                   {'event':'entity_kill'},{'event':'mob_death_notification'},{'amount':0},{'amount':2},{'amount':1.5},
                   {'pid':'unmapped'},{'victim':'minecraft:<img>'},{'victim':'minecraft:player','kind':'mob'},
                   {'victim':'minecraft:cow','kind':'player'},{'kind':'unknown'},
                   {'patch':{'damage_cause':3}},{'patch':{'damage_cause':5}},{'patch':{'damage_cause':2.0}},
                   {'patch':{'source_child':True}},{'patch':{'source_block':True}},{'patch':{'source_entity':False}},
                   {'patch':{'source_ids_equal':False}},{'patch':{'source_ids_equal':1}},
                   {'patch':{'resolved_player':False}},{'patch':{'self':True}},{'patch':{'before_dead':True}},
                   {'patch':{'after_dead':False}},{'patch':{'nested':True}},
                   {'patch':{'world_death_verified':True}},{'patch':{'phase':'pre_notification'}}):
            self.kill(**kw)
        r=self.report(metric='direct_kills')
        self.assertIsNone(r['totals']['direct_kills']);self.assertEqual(r['ranking'],[])
        self.assertEqual(r['direct_kills']['types'],[])
        for raw in ('','null','[]','{bad','['*600):self.assertEqual(valid_kill_details(raw),0)

    def test_jst_ties_pagination_unknown_opposite_and_old_schema_readonly(self):
        self.player();self.player('z','Alice');self.kill(at=BASE-1);self.kill();self.kill(pid='z')
        r=self.report(metric='direct_mob_kills',limit=1)
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='direct_mob_kills',limit=1,page=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        self.assertIsNone(r['totals']['direct_player_kills'])
        self.assertEqual(self.report(period='day',date='2026-11-05')['totals']['direct_kills'],1)
        self.assertEqual(self.report(period='month',date='2026-11')['totals']['direct_kills'],3)
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:db.execute(f'PRAGMA user_version={version}')
            digest=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['direct_kills'],3)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),digest)

    def test_type_cap_observation_period_and_no_full_kill_claim(self):
        self.player()
        for i in range(105):self.kill(victim=f'addon:mob_{i:03d}')
        self.kill(at=BASE+999999999)
        r=self.report();self.assertEqual(len(r['direct_kills']['types']),100)
        self.assertEqual(r['direct_kills']['types'][0]['type'],'addon:mob_000')
        self.assertEqual(r['direct_kills']['types'][-1]['type'],'addon:mob_099')
        self.assertEqual(r['direct_kills']['observed'],{'first_ms':BASE+1000,'last_ms':BASE+1000})
        self.assertIn('全キル数',r['direct_kills']['definition']);self.assertIn('飛び道具',r['direct_kills']['definition'])

    def test_authenticated_http_to_actual_japanese_dom(self):
        self.player(name='<script>alert(1)</script>');self.kill()
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
            url='/api/v1/analytics?period=all&metric=direct_kills'
            self.assertEqual(get(url)[0],401)
            status,raw=get(url,True);self.assertEqual(status,200)
            self.assertNotIn(b'private-uuid',raw)
            run=subprocess.run(['node','tests/action_dashboard_ui_tests.cjs'],input=raw.decode(),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode,0,run.stderr);self.assertIn('Action DOM PASS',run.stdout)
        finally:server.shutdown();server.server_close();thread.join(3)
