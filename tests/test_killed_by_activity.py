import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.killed_by_source import KILLED_BY_SOURCE, valid_killed_by_details
from companion.kill_source import PROJECTILE_KILL_SOURCE, valid_projectile_kill_details
from companion.server import CompanionServer
from test_analytics_companion import Fixture, fixture_writer, BASE, NOW

DETAILS = {'hook': 'Player::$die', 'phase': 'post_dead_transition', 'before_dead': False,
           'after_dead': True, 'nested': False, 'source_entity': True, 'source_child': False,
           'source_block': False, 'resolved_actor': True, 'same_level': True, 'self': False,
           'world_death_verified': False, 'damage_cause': 2, 'source_id': -19, 'damaging_id': -19}
CHILD = DETAILS | {'source_child': True, 'damage_cause': 3, 'damaging_id': 99}


class KilledByTests(Fixture):
    def observed(self, *, pid='private-uuid-a', at=BASE+1000, amount=1, item='minecraft:zombie',
                 path='direct', cause=KILLED_BY_SOURCE, kind='player_killed_by_entity', details=None,
                 raw=None, actor='player'):
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,
                cause,amount,primary_type,secondary_type,details_json) VALUES(?,?,?,?,?,?,?,?,?)''',
                (at, kind, actor, pid, cause, amount, item, path,
                 json.dumps(DETAILS if details is None else details) if raw is None else raw))

    def test_types_are_separate_from_old_deaths_and_numeric_causes(self):
        self.player()
        self.event('player_death', cause='ll26.60.player_dead_transition.v1')
        self.observed(kind='player_death', item='minecraft:player', path='cause:5')
        self.observed(cause='ll26.60.player_dead_transition.v1')
        r = self.report(metric='killed_by_entities')
        self.assertIsNone(r['killed_by']['count'])
        self.assertEqual(r['killed_by']['types'], [])
        self.assertEqual(r['ranking'], [])
        self.observed()
        self.observed(item='minecraft:skeleton', path='child_projectile', details=CHILD)
        r = self.report(metric='killed_by_entities')
        self.assertEqual(r['killed_by']['count'], 2)
        self.assertEqual(r['ranking'][0]['value'], 2)
        self.assertEqual({(t['type'], t['path']) for t in r['killed_by']['types']},
                         {('minecraft:zombie', 'direct'), ('minecraft:skeleton', 'child_projectile')})
        self.assertNotIn('private-uuid', json.dumps(r))
        self.assertNotIn('source_id', json.dumps(r['killed_by']))

    def test_projectile_kill_credit_is_separate_from_direct_and_killed_by(self):
        self.player(); self.player('z', 'Alice')
        details=CHILD | {'hook':'Mob::$die','resolved_player':True,'source_ids_equal':False}
        self.assertEqual(valid_projectile_kill_details(json.dumps(details)),1)
        args={'cause':PROJECTILE_KILL_SOURCE,'kind':'direct_kill_observation','path':'mob','details':details}
        self.observed(**args); self.observed(**(args | {'pid':'z','path':'player','item':'minecraft:player'}))
        for patch in ({'resolved_player':False},{'source_ids_equal':True},{'damaging_id':0},{'damage_cause':2}):
            self.observed(**(args | {'details':details | patch}))
        r=self.report(metric='projectile_kills',limit=1)
        self.assertEqual(r['totals']['projectile_kills'],2)
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='projectile_kills',limit=1,page=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        self.assertIsNone(r['totals']['direct_kills']);self.assertIsNone(r['totals']['killed_by_entities'])
        self.assertEqual({t['kind'] for t in r['projectile_kills']['types']},{'mob','player'})

    def test_fail_closed_ids_flags_source_type_json_and_time(self):
        self.player()
        for patch in ({'source_id': 0}, {'source_id': True}, {'damaging_id': 0},
                      {'damaging_id': 20}, {'damage_cause': 3}, {'source_child': True},
                      {'source_id': 2**63}, {'source_id': -(2**63)-1},
                      {'source_entity': False}, {'source_block': True}, {'resolved_actor': False},
                      {'same_level': False}, {'self': True}, {'nested': True}, {'before_dead': True},
                      {'after_dead': False}, {'after_dead': 1}, {'world_death_verified': True},
                      {'hook': 'Mob::$die'}, {'phase': 'before_origin'}):
            self.observed(details=DETAILS | patch)
        for args in ({'amount': 0}, {'amount': 2}, {'amount': -1}, {'amount': 1.5},
                     {'item': 'cause:5', 'path': 'cause:5'}, {'item': 'invalid'},
                     {'item': '<svg onload=x>'}, {'item': 'minecraft:x:y'}, {'item': 'bad/name:ok'},
                     {'at': NOW+1}, {'at': -1}, {'pid': 'unmapped'}, {'actor': 'system'},
                     {'path': 'child_projectile'}, {'details': CHILD}, {'cause': KILLED_BY_SOURCE+'.next'}):
            self.observed(**args)
        for raw in ('null', '[]', 'true', '{bad', '['*1200,
                    json.dumps(DETAILS)[:-1]+',"self":false}',
                    json.dumps(DETAILS)[:-1]+',"extra":NaN}',
                    json.dumps(DETAILS)[:-1]+',"extra":Infinity}',
                    json.dumps(DETAILS)[:-1]+',"source_id":NaN}'):
            self.observed(raw=raw)
            self.assertEqual(valid_killed_by_details(raw, 'direct'), 0)
        r = self.report(metric='killed_by_entities')
        self.assertIsNone(r['totals']['killed_by_entities'])
        self.assertEqual(r['ranking'], [])
        self.assertEqual(r['killed_by']['types'], [])

    def test_jst_pages_selection_and_readonly_history(self):
        self.player(); self.player('z', 'Alice')
        self.observed(at=BASE-1); self.observed(); self.observed(pid='z')
        self.assertEqual(self.report(period='day', date='2026-11-06')['killed_by']['count'], 2)
        self.assertEqual(self.report(period='day', date='2026-11-05')['killed_by']['count'], 1)
        self.assertEqual(self.report(period='month', date='2026-11')['killed_by']['count'], 3)
        r = self.report(metric='killed_by_entities', period='all', limit=1)
        self.assertEqual(r['ranking'][0]['key'], self.analytics.player_key('private-uuid-a'))
        r = self.report(metric='killed_by_entities', period='day', date='2026-11-06', limit=1, page=1)
        self.assertEqual(r['ranking'][0]['key'], self.analytics.player_key('z'))
        selected = self.report(player=self.analytics.player_key('z'))
        self.assertEqual(selected['killed_by']['count'], 1)
        for version in (1, 2, 3, 4):
            with fixture_writer(self.path) as db:
                db.execute(f'PRAGMA user_version={version}')
            before = hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['killed_by']['count'], 3)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(), before)

    def test_authenticated_http_to_safe_japanese_dom(self):
        self.player(name='<svg onload=alert(1)>')
        self.observed(); self.observed(item='minecraft:skeleton', path='child_projectile', details=CHILD)
        self.observed(cause=PROJECTILE_KILL_SOURCE,kind='direct_kill_observation',path='mob',
                      details=CHILD | {'hook':'Mob::$die','resolved_player':True,'source_ids_equal':False})
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,
                cause,amount,primary_type) VALUES(?,'item_drop_result','player','private-uuid-a',
                'll26.60.player_drop_returned_true.v1',64,'minecraft:stone')''', (BASE+1000,))
        server = CompanionServer(self.path, 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        def get(auth):
            client = http.client.HTTPConnection('127.0.0.1', server.server_port, timeout=5)
            try:
                client.request('GET', '/api/v1/analytics?period=all&metric=killed_by_entities',
                               headers={'Authorization': 'Bearer '+server.viewer_token} if auth else {})
                response = client.getresponse(); return response.status, response.read()
            finally:
                client.close()
        try:
            self.assertEqual(get(False)[0], 401)
            status, raw = get(True); self.assertEqual(status, 200)
            self.assertEqual(json.loads(raw)['killed_by']['count'], 2)
            self.assertNotIn(b'private-uuid', raw)
            run = subprocess.run(['node', 'tests/action_dashboard_ui_tests.cjs'], input=raw.decode(),
                                 text=True, capture_output=True, timeout=15,
                                 cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode, 0, run.stderr)
        finally:
            server.shutdown(); server.server_close(); thread.join(3)
