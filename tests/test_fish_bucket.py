"""Bucket-capture observations -> strict SQL/ranking/authenticated Japanese UI."""
import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.server import CompanionServer
from companion.stats_view import report, extra_details
from companion.fish_bucket_source import SOURCE, BUCKETS
from test_analytics_companion import Fixture, fixture_writer, BASE, NOW


def details(target='minecraft:cod'):
    return {'hook':'Player::interact','child_hook':'BucketItem::$_useOn',
        'phase':'return_success_bucket_gain_target_retired','scope':'main_inventory_type_total',
        'target_type':target,'bucket_type':BUCKETS[target],'hand':'main_hand',
        'entry_bucket_type':'minecraft:water_bucket','entry_bucket_units':1,
        'interaction_success':True,'bucket_swing':True,'target_retired':True,'target_after':'removed_flag','same_call_target':True,
        'bucket_calls':1,'nested':False,'slots':36,'before_units':2,'after_units':3,
        'fishing_rod':False,'durable_world_commit_verified':False}


def http_readback(path,expected=7):
    before=hashlib.sha256(path.read_bytes()).digest()
    server=CompanionServer(path,0);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        def get(auth):
            client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
            try:
                client.request('GET','/api/v1/stats?key=minecraft:fish_caught',
                    headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                response=client.getresponse();return response.status,json.loads(response.read())
            finally:client.close()
        assert get(False)[0]==401
        code,data=get(True);assert code==200
        assert data['ranking'][0]['value']==expected and data['totals']['value']==expected+1
        run=subprocess.run(['node','tests/fish_bucket_ui_tests.cjs'],input=json.dumps(data),text=True,
            capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1]);assert run.returncode==0,run.stderr
    finally:
        server.shutdown();server.server_close();thread.join(3)
    assert hashlib.sha256(path.read_bytes()).digest()==before


class FishBucketTests(Fixture):
    def row(self,target='minecraft:cod',at=BASE+1000,pid='private-uuid-a',data=None,**patch):
        row={'occurred_at_ms':at,'event_kind':'player_interaction_result','actor_kind':'player','actor_id':pid,
             'cause':SOURCE,'amount':1,'primary_type':'minecraft:fish_caught','secondary_type':target,
             'details_json':json.dumps(details(target)) if data is None else data if isinstance(data,str) else json.dumps(data)}
        row.update(patch)
        with fixture_writer(self.path) as db:
            db.execute('INSERT INTO events('+','.join(row)+') VALUES('+','.join('?' for _ in row)+')',tuple(row.values()))

    def view(self,**args):return report(self.analytics,key='minecraft:fish_caught',now_ms=NOW,**args)

    def test_all_type_mappings_periods_selection_rank_and_null(self):
        self.player();self.player('bob','Bob')
        self.assertIsNone(self.view()['totals']['value'])
        for target in BUCKETS:self.row(target)
        self.row(pid='bob')
        for period,date in (('all',None),('day','2026-11-06'),('month','2026-11')):
            r=self.view(period=period,date=date);self.assertEqual(r['totals']['value'],8)
            self.assertEqual([v['value'] for v in r['ranking']],[7,1])
            self.assertEqual(r['sources'],[SOURCE]);self.assertIn('釣り竿',r['definition'])
            self.assertIn('バケツ',r['label']);self.assertFalse(r['collection_complete'])
            selected=self.view(period=period,date=date,player=self.analytics.player_key('bob'))
            self.assertEqual(selected['totals']['value'],1);self.assertEqual(selected['ranking'],r['ranking'])
        self.assertIsNone(self.view(period='day',date='2026-11-05')['totals']['value'])
        self.assertEqual(self.view(limit=1,rank_page=1)['ranking'][0]['value'],1)

    def test_native_sql_and_python_validate_same_malformed_rows(self):
        self.player();self.row()
        self.assertEqual(extra_details(SOURCE,'minecraft:fish_caught','minecraft:cod',1,
            json.dumps(details()|{'target_after':'weak_unavailable'})),1)
        for patch in ({'hook':'BucketItem::$_useOn'},{'child_hook':'FishingHook::retrieve'},
                      {'phase':'before_origin'},{'target_type':'minecraft:salmon'},{'bucket_type':'minecraft:salmon_bucket'},
                      {'hand':'off_hand'},{'entry_bucket_type':'minecraft:bucket'},{'entry_bucket_units':True},
                      {'entry_bucket_units':2},{'interaction_success':False},{'bucket_swing':False},
                      {'target_retired':False},{'target_after':'alive'},{'same_call_target':False},{'bucket_calls':2},{'bucket_calls':True},
                      {'nested':True},{'slots':0},{'slots':129},{'slots':36.0},{'before_units':True},
                      {'before_units':-1},{'before_units':2**63},{'after_units':2},{'after_units':4},
                      {'after_units':3.0},{'fishing_rod':True},{'durable_world_commit_verified':True}):self.row(data=details()|patch)
        for raw in ('{bad','[]',json.dumps(details())[:-1]+',"nested":false}',json.dumps(details())+' '*2048):self.row(data=raw)
        self.row(at=NOW+1);self.row(amount=2);self.row(actor_kind='system');self.row(cause='ll26.60.bucket_attempt.v1')
        self.row(secondary_type='addon:cod');self.row(primary_type='minecraft:jump')
        header=(Path(__file__).resolve().parents[1]/'src/bedrock_insight/core/fish_bucket_sql.hpp').read_text(encoding='utf-8-sig')
        native=header.split('R"sql(',1)[1].split(')sql"',1)[0].replace("unixepoch('subsec')*1000",':now')
        with fixture_writer(self.path) as db:
            native_ids={r[0] for r in db.execute('SELECT event_id FROM events WHERE '+native,{'now':NOW})}
            python_ids={eid for eid,kind,actor,primary,secondary,cause,amount,at,raw in db.execute(
                'SELECT event_id,event_kind,actor_kind,primary_type,secondary_type,cause,amount,occurred_at_ms,details_json FROM events')
                if kind=='player_interaction_result' and actor=='player' and at<=NOW and cause==SOURCE and extra_details(cause,primary,secondary,amount,raw)}
        self.assertEqual(native_ids,python_ids);self.assertEqual(len(native_ids),1)
        self.assertEqual(self.view()['totals']['value'],1)

    def test_jst_boundaries_no_future_or_missing_identity_readonly(self):
        self.player();self.row(at=BASE-1);self.row(at=BASE);self.row(at=NOW+1);self.row(pid='unknown')
        before=hashlib.sha256(self.path.read_bytes()).digest()
        self.assertEqual(self.view()['totals']['value'],2)
        self.assertEqual(self.view(period='day',date='2026-11-05')['totals']['value'],1)
        self.assertEqual(self.view(period='day',date='2026-11-06')['totals']['value'],1)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)

    def test_http_json_to_actual_japanese_dom(self):
        self.player();self.player('bob','<svg onload=alert(1)>')
        for target in BUCKETS:self.row(target)
        self.row(pid='bob');http_readback(self.path)
