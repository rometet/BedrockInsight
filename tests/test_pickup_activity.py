import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.pickup_source import PICKUP_SOURCE,valid_pickup_details
from companion.server import CompanionServer
from test_analytics_companion import Fixture,fixture_writer,BASE


class PickupActivityTests(Fixture):
    def pickup(self,amount=3,item='minecraft:stone',pid='private-uuid-a',at=BASE+1000,
               cause=PICKUP_SOURCE,patch=None,secondary=''):
        d={'hook':'Player::take','phase':'return_true_inventory_delta','scope':'main_inventory_type_total',
           'entry_units':64,'slots':36,'before_units':63,'after_units':63+amount,
           'nested':False,'durable_transfer_verified':False}
        if patch:d.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,
                amount,primary_type,secondary_type,details_json)
                VALUES(?,'item_pickup_delta','player',?,?,?,?,?,?)''',
                (at,pid,cause,amount,item,secondary,json.dumps(d)))

    def test_partial_delta_not_entry_stack_or_attempts_and_selected_types(self):
        self.player();self.player('bob','Bob')
        self.pickup();self.pickup(2);self.pickup(1,'addon:fruit/red');self.pickup(7,pid='bob')
        r=self.report(metric='pickup_inventory_items')
        self.assertEqual(r['totals']['pickup_inventory_items'],13)
        self.assertEqual(r['totals']['pickup_delta_actions'],4)
        self.assertEqual([x['value'] for x in r['ranking']],[7,6])
        key=self.analytics.player_key('private-uuid-a')
        r=self.report(player=key,metric='pickup_delta_actions')
        self.assertEqual(r['item_pickups']['types'],[
            {'type':'minecraft:stone','items':5,'actions':2},{'type':'addon:fruit/red','items':1,'actions':1}])
        self.assertEqual(r['selected_player']['pickup_inventory_items'],6)
        self.assertIsNone(r['selected_player']['online'])
        self.assertNotIn('private-uuid',json.dumps(r))

    def test_invalid_source_delta_scope_and_absence_are_unknown(self):
        self.player()
        for kw in ({'cause':'ll26.60.take_inventory_delta.v2'},{'amount':0},{'amount':-1},{'amount':65},
                   {'amount':1.5},{'item':'minecraft:<script>'},{'pid':'missing'},{'secondary':'attempt'},
                   {'patch':{'slots':129}},{'patch':{'slots':True}},
                   {'patch':{'entry_units':256}},{'patch':{'entry_units':True}},
                   {'patch':{'after_units':67}},{'patch':{'before_units':-1}},
                   {'patch':{'scope':'all_containers'}},{'patch':{'phase':'before_origin'}},
                   {'patch':{'nested':0}},{'patch':{'durable_transfer_verified':True}}):
            self.pickup(**kw)
        r=self.report(metric='pickup_inventory_items')
        self.assertIsNone(r['totals']['pickup_inventory_items'])
        self.assertEqual(r['ranking'],[]);self.assertEqual(r['item_pickups']['types'],[])
        for raw in ('','null','[]','{bad','['*600):self.assertEqual(valid_pickup_details(raw,1),0)

    def test_jst_boundaries_ties_paging_and_old_schema_readonly(self):
        self.player();self.player('z','Alice')
        self.pickup(2,at=BASE-1);self.pickup(3);self.pickup(3,pid='z')
        r=self.report(metric='pickup_inventory_items',limit=1)
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='pickup_inventory_items',limit=1,page=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        self.assertEqual(self.report(period='day',date='2026-11-05')['totals']['pickup_inventory_items'],2)
        self.assertEqual(self.report(period='month',date='2026-11')['totals']['pickup_inventory_items'],8)
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:db.execute(f'PRAGMA user_version={version}')
            digest=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['pickup_inventory_items'],8)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),digest)

    def test_type_limit_and_observation_scope(self):
        self.player()
        for i in range(105):self.pickup(1,f'addon:item_{i:03d}')
        r=self.report();self.assertEqual(len(r['item_pickups']['types']),100)
        self.assertEqual(r['item_pickups']['types'][0]['type'],'addon:item_000')
        self.assertEqual(r['item_pickups']['types'][-1]['type'],'addon:item_099')
        self.assertEqual(r['item_pickups']['observed'],{'first_ms':BASE+1000,'last_ms':BASE+1000})

    def test_authenticated_http_to_actual_japanese_dom(self):
        self.player(name='<script>alert(1)</script>');self.pickup()
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
            url='/api/v1/analytics?period=all&metric=pickup_inventory_items'
            self.assertEqual(get(url)[0],401)
            status,raw=get(url,True);self.assertEqual(status,200)
            self.assertNotIn(b'private-uuid',raw)
            run=subprocess.run(['node','tests/action_dashboard_ui_tests.cjs'],input=raw.decode(),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode,0,run.stderr);self.assertIn('Action DOM PASS',run.stdout)
        finally:server.shutdown();server.server_close();thread.join(3)
