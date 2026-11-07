"""WORLD tick snapshots: no telemetry sum, player multiplication or fake clock."""
import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.server import CompanionServer
from companion.stats_view import report
from companion.world_tick_source import SOURCE, PREDICATE
from test_analytics_companion import Fixture, fixture_writer, BASE, NOW


def details(value=9,at=BASE+15000):
    return {'adapter':'Level::getCurrentServerTick','phase':'engine_tick_snapshot',
            'scope':'world_shared','aggregation':'latest_snapshot','tick_id':value,
            'observed_at_ms':at,'sample_interval_ms':5000,'player_time':False,'restart_continuity_verified':False}


def http_readback(path,expected=9):
    before=hashlib.sha256(path.read_bytes()).digest()
    server=CompanionServer(path,0)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        def get(auth):
            client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
            try:
                client.request('GET','/api/v1/stats?key=minecraft:total_world_time',
                    headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                response=client.getresponse();return response.status,json.loads(response.read())
            finally:client.close()
        assert get(False)[0]==401
        code,data=get(True);assert code==200
        assert data['totals']['value']==expected and data['metric_scope']=='world'
        assert not data['ranking'] and data['ranking_supported'] is False
        assert data['status']=='stale_snapshot' and data['aggregation']=='latest_snapshot'
        run=subprocess.run(['node','tests/world_tick_ui_tests.cjs'],input=json.dumps(data),text=True,
            capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
        assert run.returncode==0,run.stderr
    finally:
        server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before


class WorldTickTests(Fixture):
    def setUp(self):
        super().setUp()
        with fixture_writer(self.path) as db:db.execute("ALTER TABLE events ADD COLUMN session_id TEXT DEFAULT ''")

    def row(self,value=9,at=BASE+15000,run='run-fixture',data=None,**patch):
        row={'occurred_at_ms':at,'event_kind':'server_tick','actor_kind':'system','actor_id':run,
             'cause':SOURCE,'amount':value,'primary_type':'minecraft:total_world_time','secondary_type':'',
             'session_id':run,'details_json':json.dumps(details(value,at)) if data is None else data if isinstance(data,str) else json.dumps(data)}
        row.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('INSERT INTO events('+','.join(row)+') VALUES('+','.join('?' for _ in row)+')',tuple(row.values()))

    def view(self,**args):return report(self.analytics,key='minecraft:total_world_time',now_ms=NOW,**args)

    def test_latest_shared_value_restart_zero_and_not_sum_or_max(self):
        self.player();self.player('bob','Bob')
        self.row(1000000,BASE-1,'run-old');self.row(40,BASE+5000,'run-new')
        self.assertEqual(self.view()['totals']['value'],40)
        self.row(0,BASE+10000,'run-reset')
        zero=self.view();self.assertEqual(zero['totals']['value'],0)
        self.assertEqual(zero['totals']['source_rows'],1);self.assertNotEqual(zero['status'],'no_record')
        self.row();self.row(10,BASE+15000) # tie: higher event_id, not random
        for selected in (None,self.analytics.player_key('private-uuid-a'),self.analytics.player_key('bob')):
            r=self.view(player=selected);self.assertEqual(r['totals']['value'],10)
            self.assertEqual(r['ranking'],[]);self.assertFalse(r['ranking_supported'])
            self.assertEqual(r['totals']['source_rows'],1);self.assertEqual(r['metric_scope'],'world')
            self.assertTrue(all(p['value'] is None for p in r['players']))
            self.assertNotIn('run-fixture',json.dumps(r));self.assertIn('run_key',r['snapshot'])
        self.assertEqual(self.view(page=1)['types'],[])
        self.assertEqual(self.view(search='共有')['totals']['value'],10)
        self.assertIsNone(self.view(search="%' OR 1=1 --")['totals']['value'])
        with self.assertRaises(LookupError):self.view(player='0'*32)

    def test_jst_period_snapshot_not_elapsed_ticks_and_staleness(self):
        self.row(1000000,BASE-1,'run-old');self.row()
        for period,date,value in (('day','2026-11-05',1000000),('day','2026-11-06',9),('month','2026-11',9),('all',None,9)):
            r=self.view(period=period,date=date);self.assertEqual(r['totals']['value'],value)
            self.assertEqual(r['status'],'stale_snapshot');self.assertEqual(r['aggregation'],'latest_snapshot')
        self.assertIsNone(self.view(period='day',date='2026-11-04')['totals']['value'])
        live=report(self.analytics,key='minecraft:total_world_time',now_ms=BASE+15000)
        self.assertEqual(live['status'],'recorded_snapshot')
        late=report(self.analytics,key='minecraft:total_world_time',now_ms=NOW+86400000)
        self.assertEqual(late['totals']['value'],9) # no wall-clock extension

    def test_native_and_python_predicates_agree_and_exclude_invalid_sources(self):
        self.row()
        for patch in ({'tick_id':True},{'tick_id':10},{'tick_id':2**63},
                      {'observed_at_ms':float(BASE+15000)},{'observed_at_ms':BASE+14999},
                      {'aggregation':'sum'},{'scope':'player'},{'sample_interval_ms':50},
                      {'sample_interval_ms':5000.0},{'player_time':0},{'restart_continuity_verified':True},
                      {'adapter':'wall_clock'},{'phase':'before_origin'}):self.row(data=details()|patch)
        for raw in ('{bad','[]',json.dumps(details())[:-1]+',"tick_id":9}',json.dumps(details())+' '*1024):self.row(data=raw)
        self.row(-1);self.row(at=NOW+1);self.row(actor_kind='player');self.row(actor_id='different')
        self.row(session_id='');self.row(session_id='x'*129);self.row(primary_type='minecraft:play_time')
        self.row(100,cause='ll26.60.busytick.v1');self.row(100,cause='ll26.60.interval.v1')
        header=(Path(__file__).resolve().parents[1]/'src/bedrock_insight/core/world_tick_sql.hpp').read_text(encoding='utf-8-sig')
        native=header.split('R"sql(',1)[1].split(')sql"',1)[0]
        self.assertEqual(native.strip(),PREDICATE.strip())
        with fixture_writer(self.path) as db:
            a=list(db.execute('SELECT event_id FROM events WHERE '+native.replace("unixepoch('subsec')*1000",':now'),{'now':NOW}))
        self.assertEqual(len(a),1)
        before=hashlib.sha256(self.path.read_bytes()).digest()
        self.assertEqual(self.view()['totals']['value'],9)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)

    def test_empty_old_schema_and_global_without_players(self):
        self.assertEqual(self.view()['status'],'no_record')
        self.assertIsNone(self.view()['totals']['value'])
        with fixture_writer(self.path) as db:db.execute('ALTER TABLE events DROP COLUMN session_id')
        self.assertIsNone(self.view()['totals']['value'])

    def test_authenticated_http_and_actual_japanese_js_dom(self):
        self.player();self.player('bob','<svg onload=alert(1)>');self.row()
        http_readback(self.path)
