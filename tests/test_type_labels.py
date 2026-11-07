import hashlib
import http.client
import json
import socket
import subprocess
import threading
from pathlib import Path
from urllib.parse import urlencode
from companion.server import CompanionServer
from companion.stats_catalog import BASE_TYPE_LABELS,CUSTOM,type_label
from companion.stats_view import report
from companion.type_labels import DEFAULT_PATH,load_type_labels
from test_analytics_companion import Fixture,fixture_writer,BASE,NOW
from test_block_activity import mined_details


class TypeLabelTests(Fixture):
    def override(self,labels=None,raw=None):
        path=self.fixture/'labels.json'
        path.write_text(raw if raw is not None else json.dumps({'schema_version':1,'locale':'ja_JP','labels':labels or {}},ensure_ascii=False),encoding='utf-8')
        return path

    def populate(self):
        self.player()
        with fixture_writer(self.path) as db:
            for item in ('minecraft:stone','addon:widget'):
                for _ in range(3):
                    db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                        primary_type,secondary_type,details_json) VALUES(?,?,?,?,?,?,?,?,?)''',
                        (BASE+1000,'block_mined_result','player','private-uuid-a','ll26.60.gamemode_destroy_block.v1',1,
                         item,'',json.dumps(mined_details()|{'before_type':item})))

    def test_shared_file_names_unknowns_and_custom_precedence(self):
        labels=load_type_labels(DEFAULT_PATH)
        self.assertGreater(len(labels),100)
        self.assertEqual(labels['minecraft:iron_ingot'],'鉄インゴット')
        self.assertEqual(type_label('addon:widget'),'addon:widget')
        override=load_type_labels(self.override({'minecraft:stone':'星の石','addon:widget':'追加部品',
            'minecraft:damage_taken':'FAKE'}),labels)
        self.assertEqual(type_label('addon:widget',override),'追加部品')
        self.assertEqual(type_label('minecraft:stone'),'石')
        self.assertEqual(type_label('minecraft:blaze',override),'ブレイズ')
        for key,label,_ in CUSTOM:self.assertEqual(type_label('minecraft:'+key,override),label)
        with self.assertRaises(TypeError):override['minecraft:stone']='x'

    def test_malformed_bounds_duplicate_utf8_control_and_schema(self):
        bad=['{bad','[]','['*2000+']'*2000,'{"schema_version":1,"locale":"en_US","labels":{}}',
             '{"schema_version":true,"locale":"ja_JP","labels":{}}',
             '{"schema_version":1.0,"locale":"ja_JP","labels":{}}',
             '{"schema_version":1,"locale":"ja_JP","labels":{"a:b":"\\ud800"}}',
             '{"schema_version":1,"schema_version":1,"locale":"ja_JP","labels":{}}',
             '{"schema_version":1,"locale":"ja_JP","labels":{"a:b":"a","a:b":"b"}}']
        for raw in bad:
            with self.assertRaises(ValueError):load_type_labels(self.override(raw=raw))
        for key,value in (('a:b:c','x'),('minecraft:STONE','x'),('minecraft:stone',True),
            ('minecraft:stone',''),('minecraft:stone','x'*257),('minecraft:stone','a\nb'),
            ('minecraft:stone','\u0080'),('minecraft:stone','\u2028')):
            with self.assertRaises(ValueError):load_type_labels(self.override({key:value}))
        path=self.override(raw=' '* (128*1024+1))
        with self.assertRaises(ValueError):load_type_labels(path)
        with self.assertRaises(ValueError):load_type_labels(self.override({f'a:k{i}':'x' for i in range(4097)}))
        path=self.override({f'a:k{i}':'x' for i in range(4096)})
        self.assertEqual(len(load_type_labels(path)),4096)
        with self.assertRaises(ValueError):load_type_labels(path,BASE_TYPE_LABELS)
        path.write_bytes(b'{"schema_version":1,"locale":"ja_JP","labels":{"a:b":"\xc0\xaf"}}')
        with self.assertRaises(ValueError):load_type_labels(path)
        self.assertEqual(type_label('minecraft:stone'),'石')

    def test_invalid_override_rejected_before_bind(self):
        path=self.override(raw='{bad')
        with socket.socket() as occupied:
            occupied.bind(('127.0.0.1',0));occupied.listen()
            # ValueError, not a bind error: validate before acquiring a socket.
            with self.assertRaises(ValueError):CompanionServer(self.path,occupied.getsockname()[1],path)

    def test_authenticated_search_labels_escaping_isolation_and_readonly(self):
        self.populate();path=self.override({'minecraft:stone':'星の石','addon:widget':'追加<部品>" &'})
        server=CompanionServer(self.path,0,path);other=CompanionServer(self.path,0)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        before=hashlib.sha256(self.path.read_bytes()).digest()
        try:
            def get(query,auth=True):
                c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
                try:
                    c.request('GET','/api/v1/stats?'+urlencode(query),headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                    r=c.getresponse();return r.status,json.loads(r.read())
                finally:c.close()
            self.assertEqual(get({'category':'mined'},False)[0],401)
            code,data=get({'category':'mined'});self.assertEqual(code,200)
            self.assertEqual(data['totals']['value'],6)
            self.assertEqual({r['type']:r['label'] for r in data['types']},
                {'minecraft:stone':'星の石','addon:widget':'追加<部品>" &'})
            self.assertEqual(get({'category':'mined','search':'星の石'})[1]['totals']['value'],3)
            self.assertEqual(get({'category':'mined','type_labels':str(path)})[0],400)
            untouched=report(other.analytics,category='mined',now_ms=NOW,type_labels=other.type_labels)
            self.assertEqual({r['type']:r['label'] for r in untouched['types']}['minecraft:stone'],'石')
            result=subprocess.run(['node','tests/type_labels_ui_tests.cjs'],input=json.dumps(data),text=True,
                capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(result.returncode,0,result.stderr)
        finally:server.shutdown();server.server_close();thread.join(3);other.server_close()
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)
