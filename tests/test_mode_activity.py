import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.mode_source import MODE_SOURCE,valid_mode_details
from companion.server import CompanionServer
from test_analytics_companion import Fixture,fixture_writer,BASE


class ModeActivityTests(Fixture):
    def mode(self,mode='sprint',edge='start',amount=2,at=BASE+1000,first=None,
             pid='private-uuid-a',cause=MODE_SOURCE,patch=None):
        d={'phase':'delayed_flag_observation','poll_us':250000,'flush_us':5000000,
           'coalesced':True,'duration_measured':False,'first_ms':at if first is None else first,
           'last_ms':at,'jst_day':(at+32400000)//86400000}
        if patch:d.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,
                cause,amount,primary_type,secondary_type,details_json)
                VALUES(?,'player_mode_observation','player',?,?,?,?,?,?)''',
                (at,pid,cause,amount,mode,edge,json.dumps(d)))

    def test_aggregate_amount_not_packet_or_row_count(self):
        self.player();self.mode();self.mode(amount=3,edge='stop');self.mode('sneak',amount=4)
        r=self.report(metric='sprint_starts')
        self.assertEqual(r['totals']['sprint_starts'],2)
        self.assertEqual(r['totals']['sprint_stops'],3)
        self.assertEqual(r['totals']['sneak_starts'],4)
        self.assertIsNone(r['totals']['sneak_stops'])
        self.assertEqual(r['ranking'][0]['value'],2)
        self.assertEqual(r['player_modes']['modes'],[
            {'mode':'sneak','starts':4,'stops':None},{'mode':'sprint','starts':2,'stops':3}])
        self.assertIn('短い切替',r['player_modes']['definition'])
        self.assertIsNone(r['players'][0]['online'])

    def test_source_types_json_clock_bucket_and_zero_are_not_measured(self):
        self.player()
        for kw in ({'cause':'ll26.60.mode_flag_observed.v2'},{'amount':0},{'amount':1.5},
                   {'amount':1000001},{'mode':'fly'},{'edge':'request'},{'pid':'unknown'},
                   {'patch':{'duration_measured':True}},{'patch':{'coalesced':1}},
                   {'patch':{'poll_us':1}},{'patch':{'first_ms':BASE-1}},
                   {'patch':{'last_ms':BASE+999}},{'patch':{'jst_day':False}},
                   {'patch':{'phase':'before_origin'}}):
            self.mode(**kw)
        r=self.report(metric='sprint_starts')
        self.assertIsNone(r['totals']['sprint_starts'])
        self.assertEqual(r['ranking'],[])
        self.assertEqual(r['player_modes']['modes'],[])
        for raw in ('','[]','null','{bad','['*600):
            self.assertEqual(valid_mode_details(raw,'sprint','start',BASE,1),0)

    def test_jst_boundaries_ties_selected_unknown_and_paging(self):
        self.player();self.player('z','Alice')
        self.mode(amount=2,at=BASE-1);self.mode();self.mode(pid='z')
        r=self.report(metric='sprint_starts',limit=1)
        self.assertEqual(r['totals']['sprint_starts'],4)
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='sprint_starts',limit=1,page=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        selected=self.report(player=self.analytics.player_key('z'))
        self.assertIsNone(selected['selected_player']['sprint_stops'])
        self.assertEqual(self.report(period='day',date='2026-11-05')['totals']['sprint_starts'],2)
        self.assertEqual(self.report(period='month',date='2026-11')['totals']['sprint_starts'],6)
        self.assertEqual(self.report(period='all')['totals']['sprint_starts'],6)

    def test_first_observation_within_bucket_and_old_schema_readonly(self):
        self.player();self.mode(first=BASE+100,at=BASE+1000)
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:
                db.execute(f'PRAGMA user_version={version}')
            digest=hashlib.sha256(self.path.read_bytes()).digest()
            r=self.report()
            self.assertEqual(r['player_modes']['observed'],{'first_ms':BASE+100,'last_ms':BASE+1000})
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),digest)

    def test_actual_authenticated_json_to_mode_ui_and_escaping(self):
        self.player(name='<script>alert(1)</script>');self.mode();self.mode('sneak',edge='stop',amount=1)
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
            url='/api/v1/analytics?period=all&metric=sprint_starts'
            self.assertEqual(get(url)[0],401)
            status,raw=get(url,True);self.assertEqual(status,200)
            key=json.loads(raw)['players'][0]['key']
            status,raw=get(url+'&player='+key,True);self.assertEqual(status,200)
            self.assertNotIn(b'private-uuid',raw)
            run=subprocess.run(['node','tests/action_dashboard_ui_tests.cjs'],input=raw.decode(),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertIn('Action DOM PASS',run.stdout)
        finally:server.shutdown();server.server_close();thread.join(3)
