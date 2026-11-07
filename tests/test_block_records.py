import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import sqlite3
import threading
from companion.block_source import MINED_SOURCE,MINED_V2_SOURCE,PLACED_SOURCE,PLACED_V2_SOURCE,valid_block_details
from companion.server import CompanionServer
from companion.stats_view import report
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW
from test_block_activity import mined_details,placed_details


def http_readback(path):
    before=hashlib.sha256(path.read_bytes()).digest()
    server=CompanionServer(path,0);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        def get(route,auth=True):
            c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
            try:
                c.request('GET',route,headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                r=c.getresponse();return r.status,json.loads(r.read())
            finally:c.close()
        route='/api/v1/stats?category=mined'
        if get(route,False)[0]!=401:raise AssertionError('anonymous access')
        code,data=get(route)
        if code!=200 or data['totals']['value']!=4:raise AssertionError(data)
        if {r['type']:r['value'] for r in data['types']}!={'minecraft:stone':2,'minecraft:dirt':2}:raise AssertionError('types')
        if [r['value'] for r in data['ranking']]!=[2,1,1]:raise AssertionError('rank')
        if data['sources']!=[MINED_V2_SOURCE,MINED_SOURCE]:raise AssertionError('provenance')
        code,actions=get('/api/v1/analytics?metric=mined_blocks&period=all')
        if code!=200 or actions['totals']['mined_blocks']!=4:raise AssertionError('actions differ')
        if actions['blocks_mined']['legacy_sources']!=[MINED_SOURCE]:raise AssertionError('legacy metadata')
        if 'private:block:' in json.dumps(data):raise AssertionError('private ID leak')
        run=subprocess.run(['node','tests/block_record_ui_tests.cjs'],input=json.dumps(data),text=True,
            capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
        if run.returncode:raise AssertionError(run.stderr)
    finally:server.shutdown();server.server_close();thread.join(3)
    if hashlib.sha256(path.read_bytes()).digest()!=before:raise AssertionError('DB modified')
    db=sqlite3.connect(path.resolve().as_uri()+'?mode=ro',uri=True)
    try:
        if db.execute('PRAGMA integrity_check').fetchone()!=('ok',):raise AssertionError('integrity')
    finally:db.close()


class BlockRecordTests(Fixture):
    def row(self,*,source=MINED_V2_SOURCE,data=None,kind='block_mined_result',primary='minecraft:stone',
            secondary='',pid='private:block:a',at=BASE+1000,amount=1):
        if data is None:data=mined_details()|{'same_player':True,'same_level':True}
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type,secondary_type,details_json)
                VALUES(?,?,'player',?,?,?,?,?,?)''',(at,kind,pid,source,amount,primary,secondary,json.dumps(data) if isinstance(data,dict) else data))

    def populate(self):
        self.player('private:block:a','Alice<svg>');self.player('private:block:b','Bob');self.player('private:block:c','Carol')
        self.row();self.row()
        self.row(source=MINED_SOURCE,primary='minecraft:dirt',pid='private:block:b',data=mined_details({'before_type':'minecraft:dirt'}))
        self.row(primary='minecraft:dirt',pid='private:block:c',data=mined_details({'before_type':'minecraft:dirt'})|{'same_player':True,'same_level':True})
        self.row(data={});self.row(amount=999);self.row(at=BASE+999999999999)

    def test_strict_v1_v2_contract_duplicate_bounds_and_hand_match(self):
        for source,v2 in ((MINED_SOURCE,False),(MINED_V2_SOURCE,True)):
            data=mined_details()|({'same_player':True,'same_level':True} if v2 else {})
            raw=json.dumps(data)
            self.assertTrue(valid_block_details(raw,'mined','minecraft:stone',source,''))
            self.assertFalse(valid_block_details(raw[:-1]+',"nested":false}','mined','minecraft:stone',source,''))
            self.assertFalse(valid_block_details(json.dumps(data|{'future':0}),'mined','minecraft:stone',source,''))
            self.assertFalse(valid_block_details(raw,'mined','minecraft:dirt',source,''))
            self.assertFalse(valid_block_details(raw,'mined','minecraft:stone',source,'main_hand'))
        self.assertFalse(valid_block_details(json.dumps(mined_details()),'mined','minecraft:stone',MINED_V2_SOURCE,''))
        for hand in (0,1):
            d=placed_details({'hand':hand})|{'same_player':True,'same_level':True}
            self.assertTrue(valid_block_details(json.dumps(d),'placed','minecraft:stone',PLACED_V2_SOURCE,'main_hand' if hand==0 else 'off_hand'))
            self.assertFalse(valid_block_details(json.dumps(d),'placed','minecraft:stone',PLACED_V2_SOURCE,'off_hand' if hand==0 else 'main_hand'))
        for raw in ('{bad','[]','['*2000,' '*769):self.assertFalse(valid_block_details(raw,'mined','minecraft:stone',MINED_V2_SOURCE,''))

    def test_selected_periods_rank_and_new_source_metadata(self):
        self.populate()
        result=report(self.analytics,category='mined',now_ms=NOW)
        self.assertEqual(result['totals']['value'],4)
        self.assertEqual([r['name'] for r in result['ranking']],['Alice<svg>','Bob','Carol'])
        selected=report(self.analytics,category='mined',player=self.analytics.player_key('private:block:a'),now_ms=NOW)
        self.assertEqual(selected['totals']['value'],2)
        self.assertEqual(report(self.analytics,category='mined',period='day',date='2026-11-05',now_ms=NOW)['totals']['value'],None)
        self.assertIn('旧v1',result['definition']);self.assertIn(MINED_V2_SOURCE,result['sources'])

    def test_invalid_only_and_false_frame_never_fabricate_zero(self):
        self.player('private:block:a')
        base=mined_details()|{'same_player':True,'same_level':True}
        for patch in ({'same_player':False},{'same_level':False},{'dimension':True},{'dimension':128},{'nested':True},{'world_save_verified':True}):
            self.row(data=base|patch)
        self.row(source=PLACED_V2_SOURCE);self.row(source=MINED_SOURCE,data=base)
        result=report(self.analytics,category='mined',now_ms=NOW)
        self.assertIsNone(result['totals']['value']);self.assertEqual(result['ranking'],[])

    def test_authenticated_japanese_js_and_readonly(self):self.populate();http_readback(self.path)
