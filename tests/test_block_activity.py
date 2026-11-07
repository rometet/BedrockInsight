import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.block_source import MINED_SOURCE, PLACED_SOURCE, valid_block_details
from companion.server import CompanionServer
from test_analytics_companion import Fixture, fixture_writer, BASE


def mined_details(patch=None):
    d = {'hook': 'GameMode::$destroyBlock', 'phase': 'return_true_post_state',
         'scope': 'same_position_block_source', 'before_type': 'minecraft:stone',
         'after_type': 'minecraft:air', 'dimension': 0, 'nested': False,
         'state_transition_verified': True, 'world_save_verified': False}
    if patch:
        d.update(patch)
    return d


def placed_details(patch=None):
    d = {'hook': 'GameMode::$buildBlock', 'phase': 'return_true_post_state',
         'scope': 'same_position_block_source', 'before_type': 'minecraft:air',
         'after_type': 'minecraft:stone', 'dimension': 0, 'hand': 0, 'sim_tick': False,
         'nested': False, 'state_transition_verified': True, 'world_save_verified': False}
    if patch:
        d.update(patch)
    return d


class BlockActivityTests(Fixture):
    def insert(self, event, cause, primary, details, pid='private-uuid-a', at=BASE + 1000,
               amount=1, secondary=''):
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,?,'player',?,?,?,?,?,?)''',
                (at, event, pid, cause, amount, primary, secondary, json.dumps(details)))

    def mined(self, primary='minecraft:stone', patch=None, **kw):
        self.insert('block_mined_result', MINED_SOURCE, primary, mined_details(patch), **kw)

    def placed(self, primary='minecraft:stone', secondary='main_hand', patch=None, **kw):
        self.insert('block_placed_result', PLACED_SOURCE, primary, placed_details(patch), secondary=secondary, **kw)

    def test_success_by_type_and_player_are_separate_from_attempts(self):
        self.player()
        self.player('bob', 'Bob')
        self.mined()
        self.mined()
        self.mined(primary='minecraft:dirt', patch={'before_type': 'minecraft:dirt'})
        self.mined(pid='bob')
        self.placed()
        self.placed(secondary='off_hand',patch={'hand':1})
        # Pre-origin attempt rows must never become a success count.
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type)
                VALUES(?,'audit_block_change','player','private-uuid-a','player_destroy_attempt',1,'minecraft:stone')''', (BASE + 1000,))
        r = self.report(metric='mined_blocks')
        self.assertEqual(r['totals']['mined_blocks'], 4)
        self.assertEqual(r['totals']['mined_block_actions'], 4)
        self.assertEqual(r['totals']['placed_blocks'], 2)
        self.assertEqual(r['totals']['placed_block_actions'], 2)
        self.assertEqual([x['value'] for x in r['ranking']], [3, 1])
        self.assertEqual(r['blocks_mined']['types'], [
            {'type': 'minecraft:stone', 'count': 3},
            {'type': 'minecraft:dirt', 'count': 1}])
        self.assertEqual(r['blocks_placed']['types'], [{'type': 'minecraft:stone', 'count': 2}])
        selected = self.report(player=self.analytics.player_key('private-uuid-a'))
        self.assertEqual(selected['selected_player']['mined_blocks'], 3)
        self.assertEqual(selected['selected_player']['placed_blocks'], 2)
        self.assertNotIn('private-uuid', json.dumps(r))

    def test_invalid_and_tampered_rows_are_rejected(self):
        self.player()
        # Pre-origin attempt / wrong kind / wrong cause / malformed amount.
        self.insert('audit_block_change', 'player_destroy_attempt', 'minecraft:stone', mined_details())
        self.insert('block_mined_result', MINED_SOURCE + '.future', 'minecraft:stone', mined_details())
        self.insert('block_mined_result', MINED_SOURCE, 'minecraft:stone', mined_details(), amount=0)
        self.insert('block_mined_result', MINED_SOURCE, 'minecraft:stone', mined_details(), amount=2)
        self.insert('block_mined_result', MINED_SOURCE, 'minecraft:stone', mined_details(), amount=1.5)
        self.insert('block_mined_result', MINED_SOURCE, 'minecraft:<svg>', mined_details())
        self.insert('block_mined_result', MINED_SOURCE, 'minecraft:stone', mined_details(), pid='unmapped')
        self.insert('block_mined_result', MINED_SOURCE, 'minecraft:stone', mined_details(), secondary='main_hand')
        for patch in ({'before_type': 'minecraft:air'},
                      {'after_type': 'minecraft:stone'}, {'before_type': 'minecraft:stone', 'after_type': 'minecraft:stone'},
                      {'nested': True}, {'state_transition_verified': False},
                      {'world_save_verified': True}, {'scope': 'other'},
                      {'phase': 'pre_notification'}, {'hook': 'PlayerDestroyBlockEvent'},
                      {'dimension': 999}, {'dimension': True}, {'after_type': 'minecraft:<x>'}):
            self.mined(patch=patch)
        # primary_type must equal the proven mined type.
        self.mined(primary='minecraft:dirt')
        for patch in ({'after_type': 'minecraft:air'}, {'before_type': 'minecraft:stone'},
                      {'hand': 2}, {'hand': True}, {'sim_tick': 0}, {'sim_tick': 'false'},
                      {'nested': True}, {'state_transition_verified': False},
                      {'world_save_verified': True}, {'dimension': -2},
                      {'phase': 'attempt'}):
            self.placed(patch=patch)
        self.placed(primary='minecraft:dirt')
        self.placed(secondary='unknown_hand')
        r = self.report(metric='mined_block_actions')
        self.assertIsNone(r['totals']['mined_blocks'])
        self.assertIsNone(r['totals']['placed_blocks'])
        self.assertEqual(r['ranking'], [])
        self.assertEqual(r['blocks_mined']['types'], [])
        self.assertEqual(r['blocks_placed']['types'], [])
        self.assertFalse(valid_block_details('', 'mined', 'minecraft:stone'))
        self.assertFalse(valid_block_details('{bad', 'mined', 'minecraft:stone'))
        self.assertFalse(valid_block_details('null', 'mined', 'minecraft:stone'))
        self.assertFalse(valid_block_details(json.dumps(mined_details()), 'other', 'minecraft:stone'))

    def test_periods_readonly_bounded_and_definition(self):
        self.player()
        self.mined(at=BASE - 1)
        self.mined()
        self.mined()
        self.mined(at=BASE + 999999999)  # future, excluded
        self.assertEqual(self.report(period='day', date='2026-11-06')['totals']['mined_blocks'], 2)
        self.assertEqual(self.report(period='day', date='2026-11-05')['totals']['mined_blocks'], 1)
        self.assertEqual(self.report(period='month', date='2026-11')['totals']['mined_blocks'], 3)
        for i in range(105):
            self.mined(primary=f'addon:block_{i:03d}', patch={'before_type': f'addon:block_{i:03d}'})
        r = self.report(period='all')
        self.assertEqual(len(r['blocks_mined']['types']), 100)
        self.assertEqual(r['blocks_mined']['types'][0]['count'], 3)
        self.assertIn('全破壊数', r['blocks_mined']['definition'])
        self.assertIn('全設置数', r['blocks_placed']['definition'])
        for version in (1, 2, 3, 4):
            with fixture_writer(self.path) as db:
                db.execute(f'PRAGMA user_version={version}')
            digest = hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['mined_blocks'], 108)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(), digest)

    def test_authenticated_http_to_japanese_dom(self):
        self.player(name='<script>alert(1)</script>')
        self.mined()
        self.placed()
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type)
                VALUES(?,'item_drop_result','player','private-uuid-a','ll26.60.player_drop_returned_true.v1',64,'minecraft:stone')''', (BASE + 1000,))
        server = CompanionServer(self.path, 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()

        def get(path, auth=False):
            client = http.client.HTTPConnection('127.0.0.1', server.server_port, timeout=4)
            try:
                client.request('GET', path, headers={'Authorization': 'Bearer ' + server.viewer_token} if auth else {})
                response = client.getresponse()
                return response.status, response.read()
            finally:
                client.close()
        try:
            url = '/api/v1/analytics?period=all&metric=mined_blocks'
            self.assertEqual(get(url)[0], 401)
            status, raw = get(url, True)
            self.assertEqual(status, 200)
            self.assertNotIn(b'private-uuid', raw)
            run = subprocess.run(['node', 'tests/action_dashboard_ui_tests.cjs'], input=raw.decode(), text=True,
                                 capture_output=True, timeout=15, cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn('Action DOM PASS', run.stdout)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(3)
