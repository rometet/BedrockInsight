"""Synthetic HEALTH sampling/readback; never actual engine/gameplay acceptance."""
import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading
from companion.server import CompanionServer
from companion.stats_view import report,extra_details
from companion.health_loss_source import SOURCE,KEYS
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW
from test_absorption import details as absorption_details


def details(dealt=False,child=False,after=17000):
    return absorption_details(dealt,child)|{'attribute':'HEALTH','before_milli':20000,'after_milli':after,
        'input_milli':5000,'returned_milli':3000,'health_attribute_decrease_observed':True}


def http_readback(path):
    before=hashlib.sha256(path.read_bytes()).digest()
    server=CompanionServer(path,0);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        for key in KEYS:
            def get(auth):
                c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
                try:
                    c.request('GET','/api/v1/stats?key='+key,headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                    r=c.getresponse();return r.status,json.loads(r.read())
                finally:c.close()
            assert get(False)[0]==401
            code,data=get(True);assert code==200
            assert data['totals']['value']==80 and [r['value'] for r in data['ranking']]==[50,30]
            assert data['observation_stage']=='health_attribute_delta' and data['sources']==[SOURCE]
            result=subprocess.run(['node','tests/health_loss_ui_tests.cjs'],input=json.dumps(data),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1]);assert result.returncode==0,result.stderr
    finally:server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before


class HealthLossTests(Fixture):
    def row(self,dealt=False,pid='private-uuid-a',at=BASE+1000,data=None,amount=30,**patch):
        row={'event_kind':'player_interaction_result','actor_kind':'player','actor_id':pid,'occurred_at_ms':at,
            'primary_type':KEYS[int(dealt)],'secondary_type':'minecraft:zombie' if dealt else 'minecraft:player',
            'amount':amount,'cause':SOURCE,'details_json':json.dumps(details(dealt)) if data is None else data if isinstance(data,str) else json.dumps(data)}
        row.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('INSERT INTO events('+','.join(row)+') VALUES('+','.join('?' for _ in row)+')',tuple(row.values()))
    def view(self,dealt=False,**kw):return report(self.analytics,key=KEYS[int(dealt)],now_ms=NOW,**kw)
    def populate(self):
        self.player();self.player('bob','<Bob>')
        for dealt in (False,True):
            self.row(dealt);self.row(dealt,data=details(dealt,True,18000),amount=20);self.row(dealt,pid='bob')

    def test_values_ranking_units_pages_no_legacy_or_absorption_mixing(self):
        self.populate()
        for dealt in (False,True):
            self.row(dealt,cause='ll26.60.hurt_absorption_delta.v1',data=absorption_details(dealt),amount=20)
            self.row(dealt,event_kind='player_damage_dealt' if dealt else 'player_damage_taken',
                cause='ll26.60.actor_hurt_real_damage.v1' if dealt else 'll26.60.actor_hurt_event.v1',amount=999)
            r=self.view(dealt);self.assertEqual(r['totals']['value'],80)
            self.assertEqual([x['value'] for x in r['ranking']],[50,30]);self.assertEqual(r['unit'],'1/10HP')
            self.assertEqual(r['observation_stage'],'health_attribute_delta');self.assertIn('HEALTH',r['definition'])
            self.assertEqual(self.view(dealt,limit=1,rank_page=1)['ranking'][0]['value'],30)
            self.assertEqual(self.view(dealt,player=self.analytics.player_key('bob'))['totals']['value'],30)
            self.assertIsNone(self.view(dealt,period='day',date='2026-11-05')['totals']['value'])

    def test_strict_types_lifetime_attribution_rounding_and_malformed(self):
        self.player()
        for dealt in (False,True):
            self.row(dealt)
            for patch in ({'attribute':'ABSORPTION'},{'health_attribute_decrease_observed':False},
                {'health_attribute_decrease_observed':1},{'hook':'ActorHurtEvent'},{'phase':'pre_origin_amount'},
                {'result_type':0},{'result_type':1},{'result_type':True},{'rounding':'floor'},
                {'before_milli':True},{'before_milli':100000001},{'after_milli':-1},{'after_milli':20000},
                {'after_milli':21000},{'input_milli':1000},{'returned_milli':-1},{'parent_calls':2},
                {'same_actor':False},{'same_attribute':False},{'same_level':False},{'ambiguous':True},
                {'victim_type':'minecraft:pig'},{'role':'unknown'},{'attribution':'unknown'},
                {'damage_cause':35},{'source_id':True},{'source_id':2**63},
                {'exclusive_causation_verified':True},{'durable_world_commit_verified':True}):
                self.row(dealt,data=details(dealt)|patch)
            for raw in ('{bad','[]',json.dumps(details(dealt))[:-1]+',"same_actor":true}',
                json.dumps(details(dealt))+' '*2048):self.row(dealt,data=raw)
            self.row(dealt,amount=31);self.row(dealt,at=NOW+1);self.row(dealt,actor_kind='system')
            self.assertEqual(self.view(dealt)['totals']['value'],30)
        for child in (False,True):
            d=details(True,child)
            self.assertEqual(extra_details(SOURCE,KEYS[1],'minecraft:zombie',30,json.dumps(d)),1)
            pvp=d|{'victim_type':'minecraft:player'}
            self.assertEqual(extra_details(SOURCE,KEYS[1],'minecraft:player',30,json.dumps(pvp)),1)
        for loss,expected in ((49,0),(50,1),(149,1),(150,2)):
            self.assertEqual(extra_details(SOURCE,KEYS[0],'minecraft:player',expected,
                json.dumps(details(after=20000-loss))),int(expected>0))

    def test_unknown_legacy_only_stays_null_jst_and_readonly(self):
        self.player()
        self.row(cause='ll26.60.actor_hurt_event.v1');self.assertIsNone(self.view()['totals']['value'])
        self.row(at=BASE-1);self.row(at=BASE);self.row(at=NOW+1);self.row(pid='unknown')
        before=hashlib.sha256(self.path.read_bytes()).digest()
        self.assertEqual(self.view()['totals']['value'],60)
        for date in ('2026-11-05','2026-11-06'):self.assertEqual(self.view(period='day',date=date)['totals']['value'],30)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)

    def test_authenticated_http_and_actual_japanese_js(self):self.populate();http_readback(self.path)
