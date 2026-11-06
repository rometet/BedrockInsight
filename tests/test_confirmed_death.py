import hashlib
import http.client
import json
from pathlib import Path
import subprocess
import threading

from companion.death_source import DEATH_SOURCE, cause_number, valid_death_details
from companion.server import CompanionServer
from test_analytics_companion import Fixture, fixture_writer, BASE

DETAILS = {'hook':'Player::$die','phase':'post_dead_transition','before_dead':False,
           'after_dead':True,'killer_attribution':'unmeasured'}


class ConfirmedDeathTests(Fixture):
    def death(self, *, pid='private-uuid-a', at=BASE+1000, amount=1, code='cause:5',
              cause=DEATH_SOURCE, kind='player_death', item='minecraft:player', details=None):
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,
                actor_name,cause,amount,primary_type,secondary_type,details_json)
                VALUES(?,?,'player',?,'test',?,?,?,?,?)''',
                (at,kind,pid,cause,amount,item,code,json.dumps(DETAILS if details is None else details)))

    def test_pre_notifications_legacy_and_absent_source_are_not_confirmed(self):
        self.player();self.event('player_death_notification',cause='ll26.60.player_die_notification.v1')
        self.event('player_death')
        self.death(cause='ll26.60.player_dead_transition.v2')
        self.death(kind='player_death_notification')
        r=self.report(metric='confirmed_deaths')
        self.assertIsNone(r['totals']['confirmed_deaths'])
        self.assertIsNone(r['players'][0]['confirmed_deaths'])
        self.assertEqual(r['players'][0]['death_notifications'],1)
        self.assertEqual(r['ranking'],[])
        self.assertEqual(r['player_deaths']['causes'],[])
        self.death()
        self.assertEqual(self.report()['totals']['confirmed_deaths'],1)

    def test_positive_source_postcondition_and_damage_cause_without_killer(self):
        self.player();self.death();self.death(code='cause:-1');self.death(code='cause:3')
        r=self.report(metric='confirmed_deaths')
        self.assertEqual(r['totals']['confirmed_deaths'],3)
        self.assertEqual(r['ranking'][0]['value'],3)
        self.assertEqual({c['label'] for c in r['player_deaths']['causes']},{'落下','飛び道具','不明／未指定'})
        self.assertIn('キラー帰属',r['player_deaths']['definition'])
        self.assertIsNone(r['players'][0]['online'])
        self.assertNotIn('private-uuid',json.dumps(r))

    def test_invalid_post_state_causes_and_amounts_are_excluded(self):
        self.player()
        for patch in ({'before_dead':True},{'after_dead':False},{'after_dead':1},
                      {'before_dead':0},{'phase':'before_origin'},{'hook':'Actor::isAlive'},
                      {'killer_attribution':'guessed'}):
            self.death(details=DETAILS | patch)
        for args in ({'amount':2},{'amount':0},{'amount':-1},{'amount':1.5},
                     {'code':'cause:35'},{'code':'cause:00'},{'code':'cause:999'},
                     {'item':'minecraft:zombie'},{'pid':'unmapped-private-id'}):
            self.death(**args)
        for raw in ('', '[]', 'null', '{bad', '['*600, '{"after_dead":NaN}'):
            self.assertEqual(valid_death_details(raw),0)
        self.assertEqual(cause_number('cause:34'),34)
        self.assertIsNone(cause_number('cause:-2'))
        self.assertIsNone(self.report()['totals']['confirmed_deaths'])

    def test_jst_period_selected_causes_rank_ties_and_readonly_old_schemas(self):
        self.player();self.player('z','Alice')
        self.death(at=BASE-1);self.death();self.death(pid='z',code='cause:8')
        today=self.report(period='day',date='2026-11-06',metric='confirmed_deaths',limit=1)
        self.assertEqual(today['totals']['confirmed_deaths'],2)
        self.assertEqual(today['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        page=self.report(metric='confirmed_deaths',page=1,limit=1)
        self.assertEqual(page['ranking'][0]['key'],self.analytics.player_key('z'))
        selected=self.report(player=self.analytics.player_key('z'))
        self.assertEqual([x['code'] for x in selected['player_deaths']['causes']],['cause:8'])
        self.assertEqual(self.report(period='day',date='2026-11-05')['totals']['confirmed_deaths'],1)
        self.assertEqual(self.report(period='month',date='2026-11')['totals']['confirmed_deaths'],3)
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:
                db.execute(f'PRAGMA user_version={version}')
            before=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['confirmed_deaths'],3)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)

    def test_real_authenticated_http_json_to_japanese_actions_dom(self):
        self.player(name='<svg onload=alert(1)>')
        self.death();self.death(code='cause:-1');self.death(code='cause:3')
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,
                cause,amount,primary_type) VALUES(?,'item_drop_result','player','private-uuid-a',
                'll26.60.player_drop_returned_true.v1',64,'minecraft:stone')''',(BASE+1000,))
        server=CompanionServer(self.path,0)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        def get(path,auth=False):
            client=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=4)
            try:
                client.request('GET',path,headers={'Authorization':'Bearer '+server.viewer_token} if auth else {})
                response=client.getresponse();return response.status,response.read()
            finally:
                client.close()
        try:
            url='/api/v1/analytics?period=all&metric=confirmed_deaths'
            self.assertEqual(get(url)[0],401)
            status,raw=get(url,True);self.assertEqual(status,200)
            key=json.loads(raw)['players'][0]['key']
            status,raw=get(url+'&player='+key,True);self.assertEqual(status,200)
            data=json.loads(raw);self.assertEqual(data['selected_player']['confirmed_deaths'],3)
            self.assertNotIn(b'private-uuid',raw)
            run=subprocess.run(['node','tests/action_dashboard_ui_tests.cjs'],input=raw.decode(),
                text=True,capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertIn('Action DOM PASS',run.stdout)
        finally:
            server.shutdown();server.server_close();thread.join(3)
