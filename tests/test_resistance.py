"""Correlated stage-derived resistance, not final HP loss or applied mitigation."""
import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading
from companion.server import CompanionServer
from companion.stats_view import report,extra_details
from companion.resistance_source import SOURCE,KEYS
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW

def details(dealt=False,child=False,basis=5000,amplifier=1):
    return {'hook':'Mob::$_hurt' if dealt else 'Player::$_hurt','stage_hook':'Mob::$getDamageAfterEnchantReduction',
        'phase':'processed_hurt_resistance_formula','calculation':'post_enchant_milli_times_capped_level_over_5',
        'rounding':'nearest_milli_then_tenth','effect_id':11,'amplifier':amplifier,'duration_ticks':600,
        'hurt_input_milli':10000,'stage_input_milli':8000,'basis_milli':basis,'returned_milli':4000,
        'result_type':2,'parent_calls':0 if dealt else 1,'stage_calls':1,'same_actor':True,'same_level':True,
        'same_source':True,'same_effect':True,'ambiguous':False,'role':'attacker' if dealt else 'victim',
        'victim_type':'minecraft:zombie' if dealt else 'minecraft:player',
        'attribution':('child_projectile' if child else 'direct') if dealt else 'victim',
        'damage_cause':3 if child else 2,'source_id':100 if dealt else 0,'damaging_id':(200 if child else 100) if dealt else 0,
        'resistance_applied_measured':False,'final_health_loss_measured':False,'durable_world_commit_verified':False}

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
            assert data['totals']['value']==50 and [r['value'] for r in data['ranking']]==[30,20]
            result=subprocess.run(['node','tests/resistance_ui_tests.cjs'],input=json.dumps(data),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1]);assert result.returncode==0,result.stderr
    finally:server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before

class ResistanceTests(Fixture):
    def row(self,dealt=False,pid='private-uuid-a',at=BASE+1000,data=None,amount=20,**patch):
        row={'event_kind':'player_interaction_result','actor_kind':'player','actor_id':pid,'occurred_at_ms':at,
            'primary_type':KEYS[int(dealt)],'secondary_type':'minecraft:zombie' if dealt else 'minecraft:player',
            'amount':amount,'cause':SOURCE,'details_json':json.dumps(details(dealt)) if data is None else data if isinstance(data,str) else json.dumps(data)}
        row.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('INSERT INTO events('+','.join(row)+') VALUES('+','.join('?' for _ in row)+')',tuple(row.values()))
    def view(self,dealt=False,**kw):return report(self.analytics,key=KEYS[int(dealt)],now_ms=NOW,**kw)
    def populate(self):
        self.player();self.player('bob','<svg onload=alert(1)>')
        for dealt in (False,True):
            self.row(dealt);self.row(dealt,data=details(dealt,True,2500),amount=10);self.row(dealt,pid='bob')
    def test_roles_periods_ranking_player_pages_and_null(self):
        self.player()
        for dealt in (False,True):self.assertIsNone(self.view(dealt)['totals']['value'])
        self.populate()
        for dealt in (False,True):
            for period,date in (('all',None),('day','2026-11-06'),('month','2026-11')):
                r=self.view(dealt,period=period,date=date)
                self.assertEqual(r['totals']['value'],50);self.assertEqual([x['value'] for x in r['ranking']],[30,20])
                self.assertIn('式・丸め',r['label']);self.assertIn('実測した値ではありません',r['definition'])
                self.assertEqual(r['sources'],[SOURCE]);self.assertFalse(r['resistance_applied_measured'])
                self.assertEqual(r['observation_stage'],'derived_resistance_formula');self.assertFalse(r['collection_complete'])
            self.assertEqual(self.view(dealt,player=self.analytics.player_key('bob'))['totals']['value'],20)
            self.assertEqual(self.view(dealt,limit=1,rank_page=1)['ranking'][0]['value'],20)
            self.assertIsNone(self.view(dealt,period='day',date='2026-11-05')['totals']['value'])
    def test_native_sql_python_equivalence_rejects_uncorrelated_tampered_rows(self):
        self.player()
        for dealt in (False,True):
            self.row(dealt)
            for patch in ({'hook':'ActorHurtEvent'},{'stage_hook':'guessed'},{'phase':'pre_origin'},
                {'calculation':'all_damage'},{'rounding':'floor'},{'effect_id':True},{'effect_id':12},
                {'amplifier':-1},{'amplifier':256},{'amplifier':True},{'duration_ticks':0},{'duration_ticks':-1},{'duration_ticks':2**31},
                {'hurt_input_milli':0},{'hurt_input_milli':100000001},{'stage_input_milli':10002},{'stage_input_milli':True},
                {'basis_milli':8002},{'basis_milli':0},{'basis_milli':5000.0},{'returned_milli':-1},{'returned_milli':2**63},
                {'result_type':0},{'result_type':1},{'result_type':True},{'parent_calls':2},{'stage_calls':0},{'stage_calls':2},
                {'same_actor':False},{'same_level':False},{'same_source':False},{'same_effect':False},{'ambiguous':True},
                {'role':'unknown'},{'attribution':'unknown'},{'victim_type':'minecraft:pig'},
                {'damage_cause':35},{'source_id':2**63},{'damaging_id':2**63},
                {'resistance_applied_measured':True},{'final_health_loss_measured':True},{'durable_world_commit_verified':True}):
                self.row(dealt,data=details(dealt)|patch)
            for raw in ('{bad','[]',json.dumps(details(dealt))[:-1]+',"same_source":true}',json.dumps(details(dealt))+' '*2048):self.row(dealt,data=raw)
            self.row(dealt,amount=21);self.row(dealt,at=NOW+1);self.row(dealt,actor_kind='system');self.row(dealt,cause='guess')
            for actor_type in ('addon:Fake','a/b:c','a:b:c','a:bad"type'):
                self.row(dealt,secondary_type=actor_type,data=details(dealt)|{'victim_type':actor_type})
            self.assertEqual(self.view(dealt)['totals']['value'],20)
        for patch in ({'source_id':0},{'damaging_id':0},{'damage_cause':3},{'attribution':'child_projectile'}):self.row(True,data=details(True)|patch)
        header=(Path(__file__).resolve().parents[1]/'src/bedrock_insight/core/resistance_sql.hpp').read_text(encoding='utf-8-sig')
        native=header.split('R"sql(',1)[1].split(')sql"',1)[0].replace("unixepoch('subsec')*1000",':now')
        with fixture_writer(self.path) as db:
            native_ids={r[0] for r in db.execute('SELECT event_id FROM events WHERE '+native,{'now':NOW})}
            python_ids={eid for eid,kind,actor,primary,secondary,cause,amount,at,raw in db.execute(
                'SELECT event_id,event_kind,actor_kind,primary_type,secondary_type,cause,amount,occurred_at_ms,details_json FROM events')
                if kind=='player_interaction_result' and actor=='player' and at<=NOW and cause==SOURCE and extra_details(cause,primary,secondary,amount,raw)}
        self.assertEqual(native_ids,python_ids);self.assertEqual(len(native_ids),2)
    def test_capped_levels_rounding_bounds_and_return_zero(self):
        for amp in range(256):
            amount=10*min(amp+1,5)
            self.assertEqual(extra_details(SOURCE,KEYS[0],'minecraft:player',amount,json.dumps(details(amplifier=amp))),1)
        for basis,amount in ((249,0),(250,1),(749,1),(750,2)):
            self.assertEqual(extra_details(SOURCE,KEYS[0],'minecraft:player',amount,json.dumps(details(basis=basis,amplifier=0))),int(amount>0))
        d=details()|{'returned_milli':0}
        self.assertEqual(extra_details(SOURCE,KEYS[0],'minecraft:player',20,json.dumps(d)),1)
        d=details()|{'hook':'Mob::$_hurt'}
        self.assertEqual(extra_details(SOURCE,KEYS[0],'minecraft:player',20,json.dumps(d)),0)
        d['parent_calls']=0;self.assertEqual(extra_details(SOURCE,KEYS[0],'minecraft:player',20,json.dumps(d)),1)
    def test_jst_future_unknown_and_no_write(self):
        self.player();self.row(at=BASE-1);self.row(at=BASE);self.row(at=NOW+1);self.row(pid='unknown')
        before=hashlib.sha256(self.path.read_bytes()).digest();self.assertEqual(self.view()['totals']['value'],40)
        for date in ('2026-11-05','2026-11-06'):self.assertEqual(self.view(period='day',date=date)['totals']['value'],20)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)
    def test_actual_japanese_js_from_authenticated_http(self):self.populate();http_readback(self.path)
