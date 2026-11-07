import hashlib
import http.client
import json
from pathlib import Path
import sqlite3
import subprocess
import threading
from companion.broken_source import V1,V2,valid_broken_details
from companion.server import CompanionServer
from companion.stats_view import report
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW
from test_stats_view import BROKEN


def v2(location='main_inventory',slot=0,**changes):
    d={k:v for k,v in BROKEN.items() if k not in ('durability_exhausted_verified','max_damage_value_exceeded_read')}
    d.update(scope={'main_inventory':'main_inventory_same_slot','offhand':'offhand_same_slot','armor':'armor_same_slot'}[location],
        slots={'main_inventory':36,'offhand':1,'armor':4}[location],slot=slot,item_type='minecraft:iron_pickaxe',
        same_player=True,same_level=True,same_slot=True,dimension=0,threshold_input_reached=True,
        final_break_verified=False,world_save_verified=False)
    d.update(changes)
    return d


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
        route='/api/v1/stats?category=broken'
        if get(route,False)[0]!=401:raise AssertionError('anonymous access')
        code,data=get(route)
        if code!=200 or data['totals']['value']!=5:raise AssertionError(data)
        if {r['type']:r['value'] for r in data['types']}!={'minecraft:iron_pickaxe':2,'minecraft:shield':1,'minecraft:iron_helmet':1,'minecraft:iron_boots':1}:raise AssertionError('types')
        if [r['value'] for r in data['ranking']]!=[3,1,1] or data['sources']!=[V2,V1]:raise AssertionError('rank/source')
        code,actions=get('/api/v1/analytics?metric=broken_items&period=all')
        if code!=200 or actions['totals']['broken_items']!=5 or actions['broken_items']['source']!=V2:raise AssertionError('actions differ')
        if 'private:broken:' in json.dumps(data):raise AssertionError('ID leak')
        run=subprocess.run(['node','tests/broken_record_ui_tests.cjs'],input=json.dumps(data),text=True,capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
        if run.returncode:raise AssertionError(run.stderr)
    finally:server.shutdown();server.server_close();thread.join(3)
    if hashlib.sha256(path.read_bytes()).digest()!=before:raise AssertionError('DB modified')
    with sqlite3.connect(path.resolve().as_uri()+'?mode=ro',uri=True) as db:
        if db.execute('PRAGMA integrity_check').fetchone()!=('ok',):raise AssertionError('integrity')
    db.close()


class BrokenRecordTests(Fixture):
    def row(self,*,cause=V2,details=None,primary='minecraft:iron_pickaxe',secondary='main_inventory',
            amount=1,pid='private:broken:a',at=BASE+1000,kind='item_broken_result'):
        if details is None:details=v2()
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type,secondary_type,details_json)
                VALUES(?,?,'player',?,?,?,?,?,?)''',(at,kind,pid,cause,amount,primary,secondary,json.dumps(details) if isinstance(details,dict) else details))

    def populate(self):
        self.player('private:broken:a','Alice<svg>');self.player('private:broken:b','Bob');self.player('private:broken:c','Carol')
        self.row();self.row(primary='minecraft:shield',secondary='offhand',details=v2('offhand',item_type='minecraft:shield'))
        self.row(primary='minecraft:iron_helmet',secondary='armor',details=v2('armor',1,item_type='minecraft:iron_helmet'))
        self.row(cause=V1,secondary='',details=BROKEN,pid='private:broken:b')
        self.row(primary='minecraft:iron_boots',secondary='armor',pid='private:broken:c',details=v2('armor',3,item_type='minecraft:iron_boots'))
        self.row(details={});self.row(amount=999);self.row(at=BASE+999999999999)

    def test_exact_contract_v1_history_v2_every_armor_slot_and_bad_types(self):
        self.assertTrue(valid_broken_details(json.dumps(BROKEN),'minecraft:iron_pickaxe',V1,''))
        for location in ('main_inventory','offhand','armor'):
            for slot in range(4 if location=='armor' else 1):
                d=v2(location,slot);raw=json.dumps(d)
                self.assertTrue(valid_broken_details(raw,'minecraft:iron_pickaxe',V2,location))
                for patch in ({'same_player':False},{'same_level':False},{'same_slot':False},{'slot':True},{'slot':0.0},
                    {'damage_before':2},{'threshold_input_reached':False},{'final_break_verified':True},
                    {'world_save_verified':True},{'dimension':128},{'nested':True},{'unknown':0}):
                    self.assertFalse(valid_broken_details(json.dumps(d|patch),'minecraft:iron_pickaxe',V2,location),patch)
                self.assertFalse(valid_broken_details(raw[:-1]+',"nested":false}','minecraft:iron_pickaxe',V2,location))
                self.assertFalse(valid_broken_details(raw,'minecraft:diamond',V2,location))
        for raw in ('{}','[]','['*2000,' '*1025,'{bad'):
            self.assertFalse(valid_broken_details(raw,'minecraft:iron_pickaxe',V2,'main_inventory'))
        self.assertFalse(valid_broken_details(json.dumps(v2(scope=None)),'minecraft:iron_pickaxe',V2,'unknown'))
        self.assertFalse(valid_broken_details(json.dumps(v2('offhand',slot=1)),'minecraft:iron_pickaxe',V2,'offhand'))

    def test_personal_period_rank_history_and_unknown_not_zero(self):
        self.populate();r=report(self.analytics,category='broken',now_ms=NOW)
        self.assertEqual(r['totals']['value'],5);self.assertEqual([v['value'] for v in r['ranking']],[3,1,1])
        self.assertEqual(r['sources'],[V2,V1]);self.assertIn('防具4枠',r['definition']);self.assertIn('最終保存',r['definition'])
        selected=report(self.analytics,category='broken',player=self.analytics.player_key('private:broken:a'),now_ms=NOW)
        self.assertEqual(selected['totals']['value'],3)
        self.assertEqual(report(self.analytics,category='broken',period='day',date='2026-11-05',now_ms=NOW)['totals']['value'],None)
        self.assertEqual(report(self.analytics,category='broken',period='month',date='2026-11',now_ms=NOW)['totals']['value'],5)
        self.player('private:broken:d','InvalidOnly');self.row(pid='private:broken:d',details={})
        bad=report(self.analytics,category='broken',player=self.analytics.player_key('private:broken:d'),now_ms=NOW)
        self.assertIsNone(bad['totals']['value']);self.assertEqual(len(bad['ranking']),3)

    def test_empty_attempt_future_fractional_or_legacy_wrong_evidence_excluded(self):
        self.player('private:broken:a')
        for args in ({'details':{}},{'kind':'item_hurt_slot_loss'},{'amount':1.25},{'amount':2},
            {'secondary':'armor'},{'primary':'<bad>'},{'at':BASE+999999999999},
            {'cause':V1,'details':v2(),'secondary':''},{'details':v2(damage_before=2)}):self.row(**args)
        r=report(self.analytics,category='broken',now_ms=NOW)
        self.assertIsNone(r['totals']['value']);self.assertEqual(r['ranking'],[])
        self.assertIsNone(self.report(metric='broken_items')['totals']['broken_items'])

    def test_authenticated_http_and_japanese_actual_js(self):
        self.populate();http_readback(self.path)
