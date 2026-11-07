import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.use_source import USE_SOURCE,use_loss
from companion.server import CompanionServer
from test_analytics_companion import Fixture,fixture_writer,BASE


class UseResultActivityTests(Fixture):
    def use(self,pid='private-uuid-a',item='minecraft:snowball',hand='main_hand',at=BASE+1000,
            cause=USE_SOURCE,event='item_use_result',amount=1,patch=None):
        d={'hook':'GameMode::$useItem','phase':'return_true_inventory_snapshot','scope':'main_inventory_type_total',
           'entry_units':16,'slots':36,'before_units':16,'after_units':15,'loss_units':1,'nested':False,
           'effect_completed_verified':False,'consumption_verified':False}
        if patch:d.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,?,'player',?,?,?,?,?,?)''',
                (at,event,pid,cause,amount,item,hand,json.dumps(d)))

    def test_response_and_net_decrease_are_separate_and_selected(self):
        self.player();self.player('bob','Bob');self.use();self.use()
        self.use(item='minecraft:shield',hand='off_hand',patch={'before_units':0,'after_units':0,'loss_units':0})
        self.use(pid='bob',patch={'after_units':16,'loss_units':0})
        r=self.report(metric='use_return_actions')
        self.assertEqual(r['totals']['use_return_actions'],4);self.assertEqual(r['totals']['use_inventory_loss'],2)
        self.assertEqual([x['value'] for x in r['ranking']],[3,1])
        r=self.report(player=self.analytics.player_key('private-uuid-a'),metric='use_inventory_loss')
        self.assertEqual(r['selected_player']['use_inventory_loss'],2)
        self.assertEqual(r['item_use_results']['types'],[
            {'type':'minecraft:snowball','method':'main_hand','actions':2,'items':2},
            {'type':'minecraft:shield','method':'off_hand','actions':1,'items':None}])
        self.assertIsNone(r['selected_player']['item_use_attempts'])
        self.assertNotIn('private-uuid',json.dumps(r));self.assertIsNone(r['selected_player']['online'])

    def test_invalid_source_details_attempts_false_success_and_ambiguous_delta(self):
        self.player()
        for kw in ({'event':'item_use_attempt'},{'event':'item_use'},{'cause':'legacy'},
                   {'cause':USE_SOURCE+'.future'},{'amount':0},{'amount':2},{'amount':1.5},{'hand':'unknown'},
                   {'pid':'unmapped'},{'item':'minecraft:<svg>'},
                   {'patch':{'phase':'pre_notification'}},{'patch':{'hook':'Player::$useItem'}},
                   {'patch':{'scope':'offhand'}},{'patch':{'entry_units':0}},{'patch':{'entry_units':256}},
                   {'patch':{'slots':0}},{'patch':{'slots':129}},{'patch':{'before_units':-1}},
                   {'patch':{'after_units':-1}},{'patch':{'before_units':9181}},
                   {'patch':{'before_units':17,'after_units':0,'loss_units':17}},
                   {'patch':{'loss_units':0}},{'patch':{'loss_units':True}},
                   {'patch':{'nested':True}},{'patch':{'nested':0}},
                   {'patch':{'effect_completed_verified':True}},{'patch':{'consumption_verified':True}}):
            self.use(**kw)
        r=self.report(metric='use_return_actions')
        self.assertIsNone(r['totals']['use_return_actions']);self.assertIsNone(r['totals']['use_inventory_loss'])
        self.assertEqual(r['ranking'],[]);self.assertEqual(r['item_use_results']['types'],[])
        for raw in ('','null','[]','{bad','['*600):self.assertIsNone(use_loss(raw))

    def test_zero_decrease_increase_and_period_ties_pages_readonly(self):
        self.player();self.player('z','Alice');self.use(at=BASE-1);self.use();self.use(pid='z')
        self.assertEqual(self.report(metric='use_inventory_loss',limit=1)['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='use_inventory_loss',limit=1,page=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        self.assertEqual(self.report(period='day',date='2026-11-05')['totals']['use_inventory_loss'],1)
        self.assertEqual(self.report(period='month',date='2026-11')['totals']['use_inventory_loss'],3)
        self.player('gain','gain');self.use(pid='gain',patch={'after_units':17,'loss_units':0})
        r=self.report(player=self.analytics.player_key('gain'))
        self.assertEqual(r['selected_player']['use_return_actions'],1)
        self.assertIsNone(r['selected_player']['use_inventory_loss'])
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:db.execute(f'PRAGMA user_version={version}')
            digest=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['use_return_actions'],4)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),digest)

    def test_bounded_types_future_and_definition(self):
        self.player()
        for i in range(105):self.use(item=f'addon:item_{i:03d}')
        self.use(at=BASE+999999999)
        r=self.report();self.assertEqual(len(r['item_use_results']['types']),100)
        self.assertEqual(r['item_use_results']['types'][0]['type'],'addon:item_000')
        self.assertEqual(r['item_use_results']['observed'],{'first_ms':BASE+1000,'last_ms':BASE+1000})
        self.assertIn('全用途の成功ではありません',r['item_use_results']['definition'])
        self.assertIn('全使用数',r['item_use_results']['definition'])

    def test_authenticated_http_to_japanese_dom(self):
        self.player(name='<script>alert(1)</script>');self.use()
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
            url='/api/v1/analytics?period=all&metric=use_return_actions'
            self.assertEqual(get(url)[0],401)
            status,raw=get(url,True);self.assertEqual(status,200);self.assertNotIn(b'private-uuid',raw)
            run=subprocess.run(['node','tests/action_dashboard_ui_tests.cjs'],input=raw.decode(),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode,0,run.stderr);self.assertIn('Action DOM PASS',run.stdout)
        finally:server.shutdown();server.server_close();thread.join(3)
