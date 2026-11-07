"""v2 serialized movement -> read-only SQL/HTTP/Japanese DOM; synthetic only."""
import hashlib
import http.client
import json
from pathlib import Path
import re
import subprocess
import threading

from companion.server import CompanionServer
from companion.stats_view import report
from companion.travel_source import MODES, TRAVEL_V1, TRAVEL_V2, valid_travel_v2
from test_analytics_companion import Fixture, fixture_writer, BASE, NOW


def details(mode='walk',amount=19,at=BASE+5000):
    return {'adapter':'ServerLevelTickEvent','phase':'coherent_position_window','clock':'steady',
            'mode':mode,'distance_cm':amount,'flush_interval_ms':5000,'first_ms':at-4950,'last_ms':at,
            'dimension':0,'sample_count':100,'max_gap_us':1000000,'max_step_cm':1000,
            'precision':'floor_window_cm','period_assignment':'last_observation','world_movement_verified':False}


def http_readback(path, expected_all=34, expected_walk=24):
    """Also callable on the CLOSED StorageWriter-produced native fixture."""
    before=hashlib.sha256(path.read_bytes()).digest()
    server=CompanionServer(path,0)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        def get(route,auth=True):
            client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
            try:
                headers={'Authorization':'Bearer '+server.viewer_token} if auth else {}
                client.request('GET',route,headers=headers);response=client.getresponse()
                return response.status,json.loads(response.read())
            finally:client.close()
        assert get('/api/v1/stats',False)[0]==401
        code,actions=get('/api/v1/analytics?metric=travel_cm&period=all');assert code==200
        assert actions['totals']['travel_cm']==expected_all
        code,stats=get('/api/v1/stats?key=minecraft:walk_one_cm');assert code==200
        assert stats['ranking'][0]['value']==expected_walk
        data={'actions':actions,'stats':stats,'expected_walk':expected_walk,'expected_all':expected_all}
        result=subprocess.run(['node','tests/travel_ui_tests.cjs'],input=json.dumps(data),text=True,
                              capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
        assert result.returncode==0,result.stderr
    finally:
        server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before


class TravelV2Tests(Fixture):
    def row(self,mode='walk',amount=19,at=BASE+5000,data=None,cause=TRAVEL_V2,pid='private-uuid-a'):
        payload=details(mode,amount,at) if data is None else data
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,'player_travel_distance','player',?,?,?,?,'',?)''',
                (at,pid,cause,amount,mode,payload if isinstance(payload,str) else json.dumps(payload)))

    def test_all_seventeen_modes_period_rankings_legacy_and_readonly(self):
        self.player();self.player('bob','Bob')
        for mode in MODES:self.row(mode)
        self.row(amount=10,pid='bob')
        old={'adapter':'ServerLevelTickEvent','phase':'polled_position_delta','mode':'walk','distance_cm':5,'flush_interval_ms':5000}
        self.row(amount=5,cause=TRAVEL_V1,data=old)
        before=hashlib.sha256(self.path.read_bytes()).digest()
        for period,date in (('all',None),('day','2026-11-06'),('month','2026-11')):
            for mode in MODES:
                r=report(self.analytics,key='minecraft:'+mode+'_one_cm',period=period,date=date,now_ms=NOW)
                self.assertEqual(r['ranking'][0]['value'],24 if mode=='walk' else 19)
                self.assertIn(TRAVEL_V2,r['sources']);self.assertFalse(r['collection_complete'])
            self.assertEqual(self.analytics.report(metric='travel_cm',period=period,date=date,now_ms=NOW)['totals']['travel_cm'],338)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)

    def test_native_sql_predicate_and_python_contract_agree_on_malformed_rows(self):
        self.player();good=details()
        self.row()
        for patch in ({'phase':'polled_position_delta'},{'clock':'wall'},{'mode':'fly'},
                      {'distance_cm':True},{'distance_cm':20},{'first_ms':-1},{'first_ms':BASE-1},
                      {'first_ms':BASE+5001},{'last_ms':BASE+4999},{'last_ms':float(BASE+5000)},
                      {'dimension':2**31},{'dimension':False},{'sample_count':0},{'sample_count':1000001},
                      {'sample_count':1.0},{'flush_interval_ms':50},{'max_gap_us':0},{'max_step_cm':64},
                      {'precision':'round_tick_cm'},{'period_assignment':'flush'},
                      {'world_movement_verified':True},{'first_ms':2**63}):
            self.row(data=good|patch)
        for raw in ('{bad','[]',json.dumps(good)[:-1]+',"distance_cm":19}',
                    json.dumps(good)[:-1]+',"other":NaN}',json.dumps(good)+' '*2048):self.row(data=raw)
        self.row(mode='ride',data=good|{'mode':'ride'})
        self.row(amount=100000001,data=good|{'distance_cm':100000001})
        self.row(at=NOW+1)
        source=(Path(__file__).resolve().parents[1]/'src/bedrock_insight/core/travel_sql.hpp').read_text(encoding='utf-8-sig')
        body=source.split('kTravelPredicate[] =',1)[1].split(';',1)[0]
        predicate=''.join(json.loads('"'+part+'"') for part in re.findall(r'"((?:[^"\\]|\\.)*)"',body))
        predicate=predicate.replace("unixepoch('subsec')*1000",':now')
        with fixture_writer(self.path) as db:
            native_ids={row[0] for row in db.execute('SELECT event_id FROM events WHERE '+predicate,{'now':NOW})}
            python_ids={eid for eid,mode,amount,at,raw in db.execute('SELECT event_id,primary_type,amount,occurred_at_ms,details_json FROM events')
                        if at<=NOW and valid_travel_v2(mode,amount,at,raw)}
        self.assertEqual(native_ids,python_ids);self.assertEqual(len(native_ids),1)
        self.assertEqual(report(self.analytics,key='minecraft:walk_one_cm',now_ms=NOW)['totals']['value'],19)

    def test_last_observation_does_not_move_on_later_read_or_midnight_flush(self):
        self.player()
        before=details(at=BASE-1);before['first_ms']=BASE-10
        after=details(at=BASE+50);after['first_ms']=BASE+25
        self.row(at=BASE-1,data=before);self.row(at=BASE+50,data=after)
        for now in (NOW,NOW+86400000):
            for date in ('2026-11-05','2026-11-06'):
                r=report(self.analytics,key='minecraft:walk_one_cm',period='day',date=date,now_ms=now)
                self.assertEqual(r['totals']['value'],19)
        self.assertIsNone(report(self.analytics,key='minecraft:walk_one_cm',period='day',date='2026-11-04',now_ms=NOW)['totals']['value'])

    def test_authenticated_actual_http_json_to_japanese_dom(self):
        self.player();self.player('bob','Bob');self.row();self.row(amount=10,pid='bob')
        old={'adapter':'ServerLevelTickEvent','phase':'polled_position_delta','mode':'walk','distance_cm':5,'flush_interval_ms':5000}
        self.row(amount=5,cause=TRAVEL_V1,data=old)
        http_readback(self.path)
