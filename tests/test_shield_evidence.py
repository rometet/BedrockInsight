"""Raw investigation data must NEVER make the unsupported shield metric numeric."""
import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading
from companion.server import CompanionServer
from companion.shield_evidence import SOURCE,inspect,parse
from companion.stats_view import report
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW

def fields():return dict(blocking=False,blocked_flag=False,previous_blocked=False,previous_damaged_blocked=False,
    invulnerable=False,dead=False,shield_time=0,damaged_shield_time=0,invulnerable_ticks=0)
def details():return dict(hook='Player::$_hurt',phase='post_origin_raw_fields',input_milli=6000,result_type=0,
    damage_cause=2,returned_milli=None,returned_present=False,may_block=True,invulnerable_to_source=False,
    before=fields(),after=fields(),same_player=True,same_level=True,ambiguous=False,sample_interval_ms=5000,
    max_tracked_players=256,rounding='nearest_milli',shield_success_verified=False,
    shield_blocked_amount_measured=False,eligible_for_statistics=False)

def http_readback(path):
    before=hashlib.sha256(path.read_bytes()).digest()
    raw=inspect(path);assert len(raw['entries'])==4 and not raw['shield_metric_available'] and not raw['full_parity_accepted']
    assert all(not e['raw_observation']['eligible_for_statistics'] for e in raw['entries'])
    server=CompanionServer(path,0);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        def get(auth):
            c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
            try:
                c.request('GET','/api/v1/stats?key=minecraft:damage_blocked_by_shield',
                    headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                r=c.getresponse();return r.status,json.loads(r.read())
            finally:c.close()
        assert get(False)[0]==401
        code,data=get(True);assert code==200 and data['status']=='unsupported' and data['totals']['value'] is None
        assert data['ranking']==[] and not data['projection_supported']
        result=subprocess.run(['node','tests/shield_unavailable_ui_tests.cjs'],input=json.dumps(data),text=True,
            capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1]);assert result.returncode==0,result.stderr
    finally:server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before

class ShieldEvidenceTests(Fixture):
    def row(self,data=None,at=BASE+1000,**patch):
        row=dict(event_kind='player_shield_evidence',cause=SOURCE,actor_kind='player',actor_id='private-uuid-a',
            actor_name='<svg onload=alert(1)>',primary_type='minecraft:shield_evidence',secondary_type='',
            amount=0,occurred_at_ms=at,details_json=json.dumps(details() if data is None else data) if not isinstance(data,str) else data)
        row.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('INSERT INTO events('+','.join(row)+') VALUES('+','.join('?' for _ in row)+')',tuple(row.values()))
    def populate(self):
        self.player();self.row()
        d=details();d['after']['blocked_flag']=True;d['after']['shield_time']=100;self.row(d)
        d['result_type']=2;d['returned_present']=True;d['returned_milli']=4000;self.row(d)
        d['ambiguous']=True;self.row(d)
    def test_raw_fields_cannot_promote_statistic_or_zero(self):
        self.populate();r=inspect(self.path,now_ms=NOW)
        self.assertEqual(len(r['entries']),4);self.assertFalse(r['shield_metric_available']);self.assertFalse(r['full_parity_accepted'])
        data=report(self.analytics,key='minecraft:damage_blocked_by_shield',now_ms=NOW)
        self.assertIsNone(data['totals']['value']);self.assertEqual(data['status'],'unsupported');self.assertEqual(data['ranking'],[])
        text=json.dumps(r);self.assertNotIn('private-uuid-a',text);self.assertIn('player_key',text)
    def test_parser_never_accepts_success_or_invalid_types(self):
        self.assertIsNotNone(parse(json.dumps(details())))
        for patch in ({'eligible_for_statistics':True},{'shield_success_verified':True},{'shield_blocked_amount_measured':True},
            {'same_player':False},{'same_level':False},{'result_type':True},{'result_type':3},{'damage_cause':2**31},
            {'input_milli':True},{'input_milli':-1},{'input_milli':100000001},{'returned_milli':1},
            {'may_block':1},{'invulnerable_to_source':0},{'sample_interval_ms':5000.0},{'max_tracked_players':True},
            {'phase':'post_origin_success'},{'rounding':'exact'},{'before':[]},{'extra':'unreviewed'}):
            self.assertIsNone(parse(json.dumps(details()|patch)))
        for section in ('before','after'):
            for patch in ({'blocked_flag':1},{'shield_time':2**63},{'shield_time':-2**63-1},
                {'invulnerable_ticks':2**31},{'extra':1}):
                d=details();d[section]|=patch;self.assertIsNone(parse(json.dumps(d)))
        for raw in ('{','[]',json.dumps(details())[:-1]+',"may_block":true}',json.dumps(details())+' '*2048):self.assertIsNone(parse(raw))
        d=details();d['returned_present']=True # present but nonfinite/out-of-bound value -> explicit null
        self.assertIsNotNone(parse(json.dumps(d)))
        d['before']['shield_time']=-2**63;self.assertIsNotNone(parse(json.dumps(d)))
    def test_bounded_subset_future_foreign_and_malformed_exclusion(self):
        self.player();self.row();self.row('{');self.row(at=NOW+1);self.row(cause='fake');self.row(amount=1)
        self.row(actor_kind='system');self.row(secondary_type='fake')
        r=inspect(self.path,now_ms=NOW);self.assertEqual(len(r['entries']),1);self.assertEqual(r['invalid_in_subset'],1)
        self.assertEqual(r['scanned_subset'],2)
        r=inspect(self.path,limit=1,now_ms=NOW);self.assertEqual(r['entries'],[]);self.assertTrue(r['has_more'])
        for value in (0,1001,True):
            with self.assertRaises(ValueError):inspect(self.path,limit=value)
    def test_cli_readonly_and_empty_is_not_pass(self):
        before=hashlib.sha256(self.path.read_bytes()).digest();r=inspect(self.path,now_ms=NOW)
        self.assertEqual(r['entries'],[]);self.assertFalse(r['shield_metric_available']);self.assertFalse(r['full_parity_accepted'])
        result=subprocess.run(['python','-m','companion.shield_evidence','--database',str(self.path)],
            capture_output=True,text=True,encoding='utf-8',timeout=15)
        self.assertEqual(result.returncode,0,result.stderr);self.assertFalse(json.loads(result.stdout)['full_parity_accepted'])
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)
    def test_authenticated_http_and_actual_js_stays_unavailable(self):self.populate();http_readback(self.path)
