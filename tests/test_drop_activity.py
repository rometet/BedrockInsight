import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from test_analytics_companion import Fixture, fixture_writer, BASE
from companion.server import CompanionServer

CAUSE = 'll26.60.player_drop_returned_true.v1'


class DropActivityTests(Fixture):
    def drop(self, amount=64, item='minecraft:stone', pid='private-uuid-a', at=BASE+1000,
             cause=CAUSE, kind='item_drop_result'):
        with fixture_writer(self.path) as db:
            db.execute("""INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,
                actor_name,cause,amount,primary_type) VALUES(?,?,'player',?,'Alice',?,?,?)""",
                (at,kind,pid,cause,amount,item))

    def test_stack_units_actions_and_selected_type_breakdown(self):
        self.player();self.player('other','Bob')
        self.drop();self.drop(2);self.drop(3,'addon:fruit/red');self.drop(9,pid='other')
        result=self.report(metric='dropped_items')
        self.assertEqual(result['totals']['dropped_items'],78)
        self.assertEqual(result['totals']['drop_actions'],4)
        self.assertEqual([r['value'] for r in result['ranking']],[69,9])
        key=result['players'][0]['key']
        selected=self.report(player=key,metric='drop_actions')
        self.assertEqual(selected['item_drops']['types'],[
            {'type':'minecraft:stone','items':66,'actions':2},
            {'type':'addon:fruit/red','items':3,'actions':1}])
        self.assertEqual(selected['selected_player']['drop_actions'],3)
        self.assertNotIn('private-uuid',json.dumps(selected))

    def test_unknown_or_malformed_sources_and_absence_are_not_zero(self):
        self.player()
        for args in ({'cause':'unknown'},{'kind':'item_use'},{'amount':0},{'amount':256},
                     {'amount':-1},{'amount':1.5},{'item':'minecraft:<img>'},
                     {'item':'mine/craft:stone'},{'item':'a:b:c'},{'item':'a'*129},
                     {'pid':'unmapped-private-uuid'}):
            self.drop(**args)
        r=self.report(metric='dropped_items')
        self.assertIsNone(r['players'][0]['dropped_items'])
        self.assertIsNone(r['totals']['drop_actions'])
        self.assertEqual(r['ranking'],[])
        self.assertEqual(r['item_drops']['types'],[])
        self.assertEqual(r['item_drops']['observed'],{'first_ms':None,'last_ms':None})

    def test_period_boundaries_and_positive_deterministic_pagination(self):
        self.player();self.player('z','Alice');self.player('b','Bob')
        self.drop(2,at=BASE-1);self.drop(3);self.drop(3,pid='z');self.drop(3,pid='b')
        today=self.report(period='day',date='2026-11-06',metric='dropped_items',limit=1)
        self.assertEqual(today['ranking'][0]['value'],3)
        self.assertEqual(today['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='dropped_items',page=1,limit=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        self.assertEqual(self.report(period='day',date='2026-11-05')['totals']['dropped_items'],2)
        self.assertEqual(self.report(period='month',date='2026-11')['totals']['dropped_items'],11)
        self.assertEqual(self.report(period='all')['totals']['dropped_items'],11)
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:
                db.execute(f'PRAGMA user_version={version}')
            digest=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['dropped_items'],11)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),digest)

    def test_type_list_is_bounded_and_lexically_tied(self):
        self.player()
        for i in range(105):
            self.drop(1,f'addon:item_{i:03d}')
        rows=self.report()['item_drops']['types']
        self.assertEqual(len(rows),100)
        self.assertEqual(rows[0]['type'],'addon:item_000')
        self.assertEqual(rows[-1]['type'],'addon:item_099')

    def test_authenticated_http_and_actual_japanese_action_dom(self):
        self.player(name='<img src=x onerror=alert(1)>');self.drop()
        server=CompanionServer(self.path,0)
        thread=threading.Thread(target=server.serve_forever,daemon=True)
        thread.start()
        def get(path, auth=False):
            client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=4)
            try:
                client.request('GET',path,headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                response=client.getresponse()
                return response.status,response.read()
            finally:
                client.close()
        try:
            url='/api/v1/analytics?period=all&metric=dropped_items'
            self.assertEqual(get(url)[0],401)
            status,raw=get(url,True);self.assertEqual(status,200)
            key=json.loads(raw)['players'][0]['key']
            status,raw=get(url+'&player='+key,True);self.assertEqual(status,200)
            self.assertNotIn(b'private-uuid',raw)
            for asset in ('/actions','/actions.js'):
                self.assertEqual(get(asset)[0],200)
            result=subprocess.run(['node','tests/action_dashboard_ui_tests.cjs'],
                input=raw.decode(),text=True,capture_output=True,timeout=15,
                cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('Action DOM PASS',result.stdout)
        finally:
            server.shutdown();server.server_close();thread.join(3)
