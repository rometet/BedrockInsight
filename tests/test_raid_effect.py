"""Post-origin effects, deliberately not raid success attribution."""
import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading
from companion.server import CompanionServer
from companion.stats_view import report,extra_details
from companion.raid_effect_source import SOURCE,EFFECTS
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW


def details(key='minecraft:raid_trigger',before=False):
    effect,duration=EFFECTS[key]
    return {'hook':'Actor::addEffect','phase':'post_origin_effect_change','effect_id':effect,
        'duration_ticks':duration,'amplifier':0,'before_present':before,
        'before_duration_ticks':100 if before else 0,'before_amplifier':0,
        'after_duration_ticks':duration,'after_amplifier':0,'same_player':True,'same_level':True,
        'nested':False,'raid_outcome_verified':False,'effect_source_verified':False,'durable_world_commit_verified':False}


def http_readback(path):
    before=hashlib.sha256(path.read_bytes()).digest()
    server=CompanionServer(path,0);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        for key in EFFECTS:
            def get(auth):
                client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
                try:
                    client.request('GET','/api/v1/stats?key='+key,
                        headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                    r=client.getresponse();return r.status,json.loads(r.read())
                finally:client.close()
            assert get(False)[0]==401
            code,data=get(True);assert code==200
            assert data['totals']['value']==3 and [r['value'] for r in data['ranking']]==[2,1]
            result=subprocess.run(['node','tests/raid_effect_ui_tests.cjs'],input=json.dumps(data),
                text=True,capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            assert result.returncode==0,result.stderr
    finally:
        server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before


class RaidEffectTests(Fixture):
    def row(self,key='minecraft:raid_trigger',pid='private-uuid-a',at=BASE+1000,data=None,**patch):
        effect=EFFECTS[key][0]
        row={'event_kind':'player_interaction_result','actor_kind':'player','actor_id':pid,'occurred_at_ms':at,
            'primary_type':key,'secondary_type':'effect_id:'+str(effect),'amount':1,'cause':SOURCE,
            'details_json':json.dumps(details(key)) if data is None else data if isinstance(data,str) else json.dumps(data)}
        row.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('INSERT INTO events('+','.join(row)+') VALUES('+','.join('?' for _ in row)+')',tuple(row.values()))

    def view(self,key='minecraft:raid_trigger',**kw):return report(self.analytics,key=key,now_ms=NOW,**kw)

    def test_both_effects_changed_only_periods_player_rank_pages_and_null(self):
        self.player();self.player('bob','Bob')
        for key in EFFECTS:
            self.assertIsNone(self.view(key)['totals']['value'])
            self.row(key);self.row(key,data=details(key,True));self.row(key,pid='bob')
            for period,date in (('all',None),('day','2026-11-06'),('month','2026-11')):
                r=self.view(key,period=period,date=date)
                self.assertEqual(r['totals']['value'],3);self.assertEqual([r['value'] for r in r['ranking']],[2,1])
                self.assertIn('証明ではありません',r['definition']);self.assertIn('付与観測',r['label'])
                self.assertEqual(r['sources'],[SOURCE]);self.assertFalse(r['collection_complete'])
            r=self.view(key,player=self.analytics.player_key('bob'));self.assertEqual(r['totals']['value'],1)
            self.assertEqual(len(r['ranking']),2);self.assertEqual(self.view(key,limit=1,rank_page=1)['ranking'][0]['value'],1)
            self.assertIsNone(self.view(key,period='day',date='2026-11-05')['totals']['value'])

    def test_native_python_source_contract_equivalence(self):
        self.player()
        for key in EFFECTS:
            self.row(key);self.row(key,data=details(key,True))
            for patch in ({'hook':'Actor::getEffect'},{'phase':'pre_origin'}, {'effect_id':True},{'effect_id':28},
                {'duration_ticks':599},{'amplifier':-1},{'amplifier':256},{'amplifier':False},
                {'before_present':1},{'before_duration_ticks':1},{'before_amplifier':1},
                {'before_present':True,'before_duration_ticks':EFFECTS[key][1]},
                {'before_present':True,'before_duration_ticks':-2},{'before_present':True,'before_duration_ticks':2**31},
                {'after_duration_ticks':599},{'after_amplifier':1},{'same_player':False},{'same_level':False},
                {'nested':True},{'raid_outcome_verified':True},{'effect_source_verified':True},
                {'durable_world_commit_verified':True}):self.row(key,data=details(key)|patch)
            for raw in ('{bad','[]',json.dumps(details(key))[:-1]+',"nested":false}',json.dumps(details(key))+' '*2048):self.row(key,data=raw)
            self.row(key,amount=2);self.row(key,at=NOW+1);self.row(key,actor_kind='system')
            self.row(key,cause='ll26.60.effect_attempt.v1');self.row(key,secondary_type='effect_id:28')
            self.assertEqual(self.view(key)['totals']['value'],2)
        header=(Path(__file__).resolve().parents[1]/'src/bedrock_insight/core/raid_effect_sql.hpp').read_text(encoding='utf-8-sig')
        native=header.split('R"sql(',1)[1].split(')sql"',1)[0].replace("unixepoch('subsec')*1000",':now')
        with fixture_writer(self.path) as db:
            native_ids={r[0] for r in db.execute('SELECT event_id FROM events WHERE '+native,{'now':NOW})}
            python_ids={eid for eid,kind,actor,primary,secondary,cause,amount,at,raw in db.execute(
                'SELECT event_id,event_kind,actor_kind,primary_type,secondary_type,cause,amount,occurred_at_ms,details_json FROM events')
                if kind=='player_interaction_result' and actor=='player' and at<=NOW and cause==SOURCE and extra_details(cause,primary,secondary,amount,raw)}
        self.assertEqual(native_ids,python_ids);self.assertEqual(len(native_ids),4)

    def test_jst_and_readonly_with_unknown_player_exclusion(self):
        self.player();self.row(at=BASE-1);self.row(at=BASE);self.row(at=NOW+1);self.row(pid='unknown')
        before=hashlib.sha256(self.path.read_bytes()).digest()
        self.assertEqual(self.view()['totals']['value'],2)
        for date in ('2026-11-05','2026-11-06'):self.assertEqual(self.view(period='day',date=date)['totals']['value'],1)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)

    def test_authenticated_http_to_japanese_dom(self):
        self.player();self.player('bob','<img src=x onerror=alert(1)>')
        for key in EFFECTS:
            self.row(key);self.row(key,data=details(key,True));self.row(key,pid='bob')
        http_readback(self.path)
