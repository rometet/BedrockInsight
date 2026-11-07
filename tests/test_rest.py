"""Rest counters are latest snapshots, not additive history or wall time."""
import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading
from companion.server import CompanionServer
from companion.stats_view import report
from companion.rest_source import PREDICATE,SOURCE
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW

KEY='minecraft:time_since_rest'
def details(ticks=200,known=True,at=BASE+1000):
    return {'hook':'ServerPlayer::$normalTick','phase':'post_origin_sleep_snapshot',
        'aggregation':'latest_per_player_snapshot','baseline_known':known,'ticks':ticks,'observed_at_ms':at,
        'sample_interval_ms':5000,'gap_limit_ms':30000,'wall_clock_estimate':False,
        'offline_time_included':False,'restart_continuity_verified':False,'full_night_sleep_verified':False}

def http_readback(path):
    before=hashlib.sha256(path.read_bytes()).digest()
    server=CompanionServer(path,0);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        def get(auth):
            c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
            try:
                c.request('GET','/api/v1/stats?key='+KEY,headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                r=c.getresponse();return r.status,json.loads(r.read())
            finally:c.close()
        assert get(False)[0]==401
        code,data=get(True);assert code==200
        assert data['totals']['value']==200 and [r['value'] for r in data['ranking']]==[200]
        result=subprocess.run(['node','tests/rest_ui_tests.cjs'],input=json.dumps(data),text=True,
            capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1]);assert result.returncode==0,result.stderr
    finally:server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before

class RestTests(Fixture):
    def row(self,ticks=200,known=True,pid='private-uuid-a',at=BASE+1000,data=None,**patch):
        row={'event_kind':'player_interaction_result','actor_kind':'player','actor_id':pid,'occurred_at_ms':at,
            'primary_type':KEY,'secondary_type':'','amount':ticks,'cause':SOURCE,
            'details_json':json.dumps(details(ticks,known,at)) if data is None else data if isinstance(data,str) else json.dumps(data)}
        row.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('INSERT INTO events('+','.join(row)+') VALUES('+','.join('?' for _ in row)+')',tuple(row.values()))
    def view(self,**kw):return report(self.analytics,key=KEY,now_ms=NOW,**kw)
    def populate(self):
        self.player();self.player('bob','<svg onload=alert(1)>');self.player('charlie','Charlie')
        self.row(100);self.row(200,at=BASE+1001);self.row(0,at=BASE+1002)
        self.row(120,pid='bob');self.row(200,pid='bob',at=BASE+1002)
        self.row(300,pid='charlie');self.row(0,False,pid='charlie',at=BASE+1002)
    def test_latest_not_sum_or_max_reset_zero_and_unknown(self):
        self.player();self.assertIsNone(self.view()['totals']['value']);self.populate()
        for period,date in (('all',None),('day','2026-11-06'),('month','2026-11')):
            r=self.view(period=period,date=date)
            self.assertEqual(r['totals']['value'],200);self.assertEqual(r['totals']['source_rows'],2)
            self.assertEqual([r['value'] for r in r['ranking']],[200]);self.assertEqual(r['aggregation'],'latest_per_player_snapshot')
            self.assertIn('最終保存値',r['label']);self.assertIn('50ms壁時計換算',r['definition']);self.assertFalse(r['collection_complete'])
        self.assertEqual(self.view(player=self.analytics.player_key('private-uuid-a'))['totals']['value'],0)
        self.assertIsNone(self.view(player=self.analytics.player_key('charlie'))['totals']['value'])
        self.assertEqual(self.view(limit=1,rank_page=1)['ranking'],[])
    def test_period_snapshot_not_clipped_duration_and_tie_order(self):
        self.player();self.player('bob','Alice')
        self.row(100000,at=BASE-1);self.row(10,at=BASE);self.row(10,pid='bob',at=BASE)
        self.assertEqual(self.view(period='day',date='2026-11-05')['totals']['value'],100000)
        r=self.view(period='day',date='2026-11-06',limit=1)
        self.assertEqual(r['totals']['value'],20);self.assertTrue(r['ranking_has_more'])
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('bob'))
        self.assertEqual(len(self.view(limit=1,rank_page=1)['ranking']),1)
        self.row(0,False,at=BASE+2)
        self.assertIsNone(self.view(player=self.analytics.player_key('private-uuid-a'))['totals']['value'])
    def test_malformed_rows_rejected_and_native_predicate_equivalent(self):
        self.player();self.row()
        for patch in ({'hook':'Player::$normalTick'},{'phase':'before_origin'},
            {'aggregation':'sum'},{'baseline_known':1},{'baseline_known':False},
            {'ticks':True},{'ticks':201},{'ticks':-1},{'observed_at_ms':True},{'observed_at_ms':0},
            {'sample_interval_ms':5000.0},{'gap_limit_ms':30000.0},{'wall_clock_estimate':True},
            {'offline_time_included':True},{'restart_continuity_verified':True},{'full_night_sleep_verified':True}):
            self.row(data=details()|patch)
        for raw in ('{','[]',json.dumps(details())[:-1]+',"ticks":200}',json.dumps(details())+' '*1024):self.row(data=raw)
        self.row(at=NOW+1);self.row(pid='unknown');self.row(actor_kind='system');self.row(secondary_type='fake')
        self.assertEqual(self.view()['totals']['value'],200)
        header=(Path(__file__).resolve().parents[1]/'src/bedrock_insight/core/rest_sql.hpp').read_text(encoding='utf-8-sig')
        native=header.split('R"sql(',1)[1].split(')sql"',1)[0].replace("unixepoch('subsec')*1000",':now')
        with fixture_writer(self.path) as db:
            self.assertEqual([r[0] for r in db.execute('SELECT event_id FROM events WHERE '+native,{'now':NOW})],
                [r[0] for r in db.execute('SELECT event_id FROM events WHERE '+PREDICATE,{'now':NOW})])
    def test_readonly_search_and_no_extrapolation(self):
        self.player();self.row(4)
        before=hashlib.sha256(self.path.read_bytes()).digest()
        r=report(self.analytics,key=KEY,now_ms=NOW+365*86400000)
        self.assertEqual(r['totals']['value'],4);self.assertEqual(r['totals']['last_ms'],BASE+1000)
        self.assertIsNone(self.view(search="' OR 1=1 --")['totals']['value'])
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)
    def test_http_real_japanese_js_dom(self):self.populate();http_readback(self.path)
