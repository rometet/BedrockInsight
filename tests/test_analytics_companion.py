"""Deterministic fixtures + actual loopback HTTP. Never write a BDS directory."""
import hashlib
import http.client
import json
from pathlib import Path
import socket
import sqlite3
import threading
import unittest
import uuid
from datetime import datetime, timezone
from contextlib import contextmanager

from companion.analytics import Analytics, period_bounds, readonly_source
from companion.server import CompanionServer

PROJECT = Path(__file__).resolve().parents[1]
BASE = int(datetime(2026, 11, 5, 15, tzinfo=timezone.utc).timestamp() * 1000)  # Nov 6 00:00 JST
NOW = BASE + 12 * 3600_000
SCHEMA = """
PRAGMA user_version=4;
CREATE TABLE players(player_id TEXT PRIMARY KEY,last_name TEXT,first_seen_ms INTEGER,last_seen_ms INTEGER);
CREATE TABLE sessions(session_id TEXT PRIMARY KEY,player_id TEXT,started_at_ms INTEGER,ended_at_ms INTEGER,duration_ms INTEGER);
CREATE TABLE events(event_id INTEGER PRIMARY KEY,occurred_at_ms INTEGER,event_kind TEXT,actor_kind TEXT,actor_id TEXT,actor_name TEXT,cause TEXT,amount INTEGER DEFAULT 1,primary_type TEXT DEFAULT '',secondary_type TEXT DEFAULT '',details_json TEXT DEFAULT '');
CREATE TABLE server_tick_samples(event_id INTEGER PRIMARY KEY,occurred_at_ms INTEGER,mspt INTEGER,online_players INTEGER);
"""


@contextmanager
def fixture_writer(path):
    db = sqlite3.connect(path)
    try:
        with db:
            yield db
    finally:
        db.close()


class Fixture(unittest.TestCase):
    def setUp(self):
        root = PROJECT / ".build" / "analytics-tests"
        root.mkdir(parents=True, exist_ok=True)
        # Python 3.13 TemporaryDirectory uses Windows owner-only ACLs that the
        # managed process sandbox cannot reopen. Inherit normal workspace ACLs;
        # never change security settings. Retain owned fixtures for inspection.
        self.fixture = root / ("run-" + uuid.uuid4().hex)
        self.fixture.mkdir()
        self.path = self.fixture / "fixture.sqlite3"
        with fixture_writer(self.path) as db:
            db.executescript(SCHEMA)
        self.analytics = Analytics(self.path, b"unit-test-only-not-a-viewer-token")

    def tearDown(self):
        pass

    def player(self, player_id="private-uuid-a", name="Alice"):
        with fixture_writer(self.path) as db:
            db.execute("INSERT OR IGNORE INTO players VALUES(?,?,?,?)", (player_id, name, BASE-10000, BASE+10000))

    def session(self, sid, start, end, pid="private-uuid-a", duration="auto"):
        with fixture_writer(self.path) as db:
            db.execute("INSERT INTO sessions VALUES(?,?,?,?,?)", (sid, pid, start, end,
                None if end is None else end-start if duration == "auto" else duration))

    def event(self, kind, at=BASE+1000, pid="private-uuid-a", cause="levilamina_player_event"):
        with fixture_writer(self.path) as db:
            db.execute("INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,actor_name,cause) VALUES(?,?,'player',?,'Alice',?)", (at, kind, pid, cause))

    def report(self, **args):
        return self.analytics.report(now_ms=NOW, **args)


class AnalyticsTests(Fixture):
    def test_jst_clipping_and_starts_not_end_day(self):
        self.player()
        self.session("overnight", BASE-10000, BASE+10000)
        self.event("session_start", BASE-10000)
        self.event("session_end", BASE+10000)
        today = self.report(period="day", date="2026-11-06")
        yesterday = self.report(period="day", date="2026-11-05")
        self.assertEqual(today["totals"]["playtime"], 10000)
        self.assertEqual(today["totals"]["sessions"], 0)
        self.assertEqual(yesterday["totals"]["playtime"], 10000)
        self.assertEqual(yesterday["totals"]["sessions"], 1)
        self.assertEqual(self.report(period="all")["totals"]["playtime"], 20000)

    def test_month_boundary_and_leap_day(self):
        jan = int(datetime(2027, 1, 1, tzinfo=timezone.utc).timestamp()*1000)
        start, end, label = period_bounds("month", "2026-12", jan)
        self.assertEqual(label, "2026-12")
        self.assertEqual(end-start, 31*86400_000)
        a, b, _ = period_bounds("day", "2024-02-29", NOW)
        self.assertEqual(b-a, 86400_000)
        for period, date in [("day", "2026-02-29"), ("day", "2026-11-07"), ("month", "2026-13"), ("all", "2026-11"), ("week", None)]:
            with self.assertRaises(ValueError):
                period_bounds(period, date, NOW)

    def test_monthly_sql_splits_time_but_counts_start_once(self):
        self.player()
        november = int(datetime(2026, 10, 31, 15, tzinfo=timezone.utc).timestamp()*1000)
        self.session("month-crossing", november-10000, november+10000)
        self.assertEqual(self.report(period="month",date="2026-10")["totals"]["playtime"],10000)
        november_report = self.report(period="month",date="2026-11")
        self.assertEqual(november_report["totals"]["playtime"],10000)
        self.assertEqual(november_report["totals"]["sessions"],0)

    def test_duplicate_nested_overlapping_sessions_union(self):
        self.player()
        for sid, a, b in [("a",0,10000),("b",1000,2000),("c",3000,12000),("d",0,10000),("e",12000,15000)]:
            self.session(sid, BASE+a, BASE+b)
        r = self.report(period="all")
        self.assertEqual(r["totals"]["playtime"], 15000)
        self.assertEqual(r["totals"]["sessions"], 5)
        self.assertEqual(r["server"]["peak_closed_lower_bound"], 1)

    def test_crash_open_invalid_and_future_do_not_grow(self):
        self.player()
        self.session("closed", BASE, BASE+10000)
        self.session("crashed", BASE-86400_000*20, None)
        self.session("bad-clock", BASE+10000, BASE, duration=0)
        self.session("bad-duration", BASE, BASE+1000, duration=9)
        self.session("future", BASE, NOW+500000)
        first = self.report(period="all")
        later = self.analytics.report(period="all", now_ms=NOW+10000)
        self.assertEqual(first["totals"]["playtime"], 10000)
        self.assertEqual(later["totals"]["playtime"], 10000)
        self.assertEqual(first["totals"]["open_sessions"], 1)
        self.assertEqual(first["totals"]["invalid_sessions"], 3)
        self.assertIsNone(first["players"][0]["online"])
        self.assertIsNone(first["players"][0]["estimated_live_ms"])

    def test_attempts_not_successes_or_generic_audit(self):
        self.player()
        self.event("audit_block_change", cause="player_destroy_attempt")
        self.event("audit_block_change", cause="player_place_attempt")
        self.event("audit_block_change", cause="explosion")
        self.event("block_break", cause="player_destroy_attempt")
        self.event("item_craft")
        r = self.report(period="all")
        self.assertEqual(r["totals"]["destroy_attempts"], 1)
        self.assertEqual(r["totals"]["place_attempts"], 1)
        # A bare item_craft event is not a proven craft; only succeeded craft
        # requests feed crafted_items, so this stays unknown (None).
        self.assertIsNone(r["totals"]["crafted_items"])
        # Real damage-dealt is now measured; absorption/resistance remain unmeasured.
        self.assertIn("吸収・耐性", " ".join(r["unmeasured"]))
        with self.assertRaises(ValueError):
            self.report(metric="kills")

    def test_rankings_positive_deterministic_paginated(self):
        self.player("private-b", "Bob")
        self.player("private-a", "Alice")
        self.player("private-z", "No data")
        for pid in ("private-b", "private-a"):
            self.session(pid, BASE, BASE+1000, pid)
        r = self.report(period="all", limit=1)
        self.assertEqual(r["ranking"][0]["name"], "Alice")
        self.assertEqual(self.report(period="all", limit=1, page=1)["ranking"][0]["name"], "Bob")
        self.assertEqual(len(self.report(period="all")["ranking"]), 2)
        self.assertNotIn("private-a", json.dumps(r))
        for args in ({"limit":101},{"page":-1},{"players_page":1001},{"metric":"playtime;DROP TABLE players"}):
            with self.assertRaises(ValueError):
                self.report(**args)

    def test_player_selection_no_raw_identifiers(self):
        self.player(name='<img src=x onerror="alert(1)">')
        r = self.report(period="all")
        key = r["players"][0]["key"]
        selected = self.report(period="all", player=key)["selected_player"]
        self.assertEqual(selected["name"], '<img src=x onerror="alert(1)">')
        self.assertNotIn("private-uuid", json.dumps(selected))
        with self.assertRaises(LookupError):
            self.report(player="f"*32)

    def test_identical_name_ties_and_half_open_activity_bounds(self):
        for pid in ("private-b","private-a"):
            self.player(pid,"Same")
            self.session(pid,BASE,BASE+1000,pid)
        self.event("session_start", BASE, "private-a")
        self.event("session_start", BASE+86400_000, "private-a")
        r = self.analytics.report(period="day",date="2026-11-06",now_ms=NOW+2*86400_000)
        self.assertEqual(r["ranking"][0]["key"],self.analytics.player_key("private-a"))
        self.assertEqual(r["players"][0]["joins"],1)

    def test_tps_no_samples_and_untrusted_legacy_samples(self):
        self.assertIsNone(self.report()["server"]["effective_tps"])
        with fixture_writer(self.path) as db:
            db.execute("INSERT INTO server_tick_samples VALUES(1,?,50,20)", (BASE,))
        r = self.report()["server"]
        self.assertEqual(r["sampling_status"], "unavailable")
        self.assertEqual(r["sample_count"], 0)
        self.assertEqual(r["legacy_tick_row_count"], 1)
        for key in ("effective_tps","processing_mspt","current_online","last_sample_ms"):
            self.assertIsNone(r[key])

    def test_readonly_wal_snapshot_sees_uncheckpointed_commits(self):
        before = self.path.read_bytes()
        with readonly_source(self.path) as db:
            with self.assertRaises(sqlite3.OperationalError):
                db.execute("INSERT INTO players VALUES('x','x',0,0)")
        self.assertEqual(before, self.path.read_bytes())
        writer = sqlite3.connect(self.path)
        try:
            writer.execute("PRAGMA journal_mode=WAL")
            writer.execute("PRAGMA wal_autocheckpoint=0")
            writer.execute("INSERT INTO players VALUES('wal-player','WAL',0,0)")
            writer.commit()
            db_hash = hashlib.sha256(self.path.read_bytes()).digest()
            wal = Path(str(self.path)+"-wal")
            wal_hash = hashlib.sha256(wal.read_bytes()).digest()
            self.assertEqual(self.report()["players"][0]["name"], "WAL")
            self.assertEqual(db_hash, hashlib.sha256(self.path.read_bytes()).digest())
            self.assertEqual(wal_hash, hashlib.sha256(wal.read_bytes()).digest())
        finally:
            writer.close()

    def test_future_schema_fails_without_migration(self):
        with fixture_writer(self.path) as db:
            db.execute("PRAGMA user_version=5")
        before = self.path.read_bytes()
        with self.assertRaises(sqlite3.DatabaseError):
            self.report()
        self.assertEqual(before, self.path.read_bytes())

    def test_required_live_wal_absent_fails_without_sidecar_creation(self):
        before = sorted(p.name for p in self.fixture.iterdir())
        with self.assertRaises(sqlite3.OperationalError):
            with readonly_source(self.path, require_wal=True):
                self.fail("missing live WAL must not be opened")
        self.assertEqual(before, sorted(p.name for p in self.fixture.iterdir()))

    def test_legacy_schema_has_no_sample_table(self):
        with fixture_writer(self.path) as db:
            db.execute("DROP TABLE server_tick_samples")
            db.execute("PRAGMA user_version=1")
        self.assertEqual(self.report()["server"]["sampling_status"],"unavailable")


class HttpTests(Fixture):
    def setUp(self):
        super().setUp()
        self.player(name='<script>alert("x")</script>')
        self.event("session_start")
        self.server = CompanionServer(self.path, 0)
        self.token = self.server.viewer_token
        self.thread = threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval":0.01})
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(3)
        super().tearDown()

    def get(self, path="/api/v1/analytics?period=all", headers=None, method="GET"):
        client = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=4)
        try:
            client.request(method, path, headers=headers or {})
            response = client.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            client.close()

    def auth(self):
        return {"Authorization":"Bearer "+self.token}

    def test_actual_authenticated_api_and_assets(self):
        self.assertEqual(self.server.server_address[0], "127.0.0.1")
        self.assertEqual(self.get()[0], 401)
        self.assertEqual(self.get(headers={"Authorization":"Bearer incorrect"})[0], 401)
        status, headers, raw = self.get(headers=self.auth())
        self.assertEqual(status, 200)
        body = json.loads(raw)
        self.assertTrue(body["companion"])
        self.assertIn('<script>alert("x")</script>', body["players"][0]["name"])
        self.assertNotIn(b"private-uuid", raw)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertIn("frame-ancestors 'none'", headers["Content-Security-Policy"])
        self.assertNotIn("Access-Control-Allow-Origin", headers)
        for path in ("/","/app.js","/styles.css"):
            self.assertEqual(self.get(path)[0], 200)
        js = self.get("/app.js")[2].decode()
        self.assertIn("textContent", js)
        for forbidden in ("innerHTML","localStorage","sessionStorage","eval("):
            self.assertNotIn(forbidden, js)

    def test_auth_host_origin_and_readonly_routes(self):
        for headers in (self.auth() | {"Host":"evil.test"}, self.auth() | {"Origin":"https://evil.test"},
                        self.auth() | {"Sec-Fetch-Site":"cross-site"}):
            self.assertEqual(self.get(headers=headers)[0], 403)
        for method in ("POST","PUT","PATCH","DELETE","OPTIONS"):
            self.assertEqual(self.get(headers=self.auth(),method=method)[0], 405)
        self.assertEqual(self.get("/api/v1/rollback",headers=self.auth())[0], 404)
        for query in ("period=all&period=day","metric=kills","limit=101","page=-1","token=secret","metric=playtime%27%3Bdrop"):
            self.assertEqual(self.get("/api/v1/analytics?"+query,headers=self.auth())[0], 400)
        self.server.token_deadline = 0
        self.assertEqual(self.get(headers=self.auth())[0], 401)

    def test_duplicate_headers_and_size_bound(self):
        client = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=4)
        client.putrequest("GET", "/api/v1/analytics", skip_host=True)
        client.putheader("Host",f"127.0.0.1:{self.server.server_port}")
        client.putheader("Authorization","Bearer "+self.token)
        client.putheader("Authorization","Bearer "+self.token)
        client.endheaders()
        response = client.getresponse()
        self.assertEqual(response.status, 401)
        response.read();client.close()
        self.assertEqual(self.get(headers=self.auth() | {"X-Large":"x"*9000})[0], 431)

    def test_partial_request_total_timeout_then_next_request(self):
        connection = socket.create_connection(self.server.server_address, timeout=4)
        connection.sendall(b"GET / HTTP/1.1\r\nHost: ")
        connection.settimeout(4)
        self.assertEqual(connection.recv(1), b"")
        connection.close()
        self.assertEqual(self.get(headers=self.auth())[0], 200)


if __name__ == "__main__":
    unittest.main()
