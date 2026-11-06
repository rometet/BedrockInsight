"""New interval provenance only; fixtures never write running BDS/companion."""
import json
import http.client
import threading
import time

from test_analytics_companion import Fixture, fixture_writer, NOW
from companion.telemetry import SAMPLE_CAUSE, END_CAUSE, START_CAUSE
from companion.server import CompanionServer


class TickTelemetryTests(Fixture):
    def setUp(self):
        super().setUp()
        with fixture_writer(self.path) as db:
            for column, definition in [("session_id", "TEXT"), ("online_players", "INTEGER")]:
                db.execute(f"ALTER TABLE events ADD COLUMN {column} {definition}")

    def sample(self, *, stamp=NOW-1000, count=100, elapsed=5000000, online=2,
               peak=3, cause=SAMPLE_CAUSE, run_id="lltick-fixture", raw=None):
        with fixture_writer(self.path) as db:
            db.execute("""INSERT INTO events(occurred_at_ms,event_kind,actor_kind,
                cause,session_id,amount,details_json,online_players)
                VALUES(?,'server_tick','system',?,?,?,?,?)""", (stamp,cause,run_id,count,
                raw if raw is not None else json.dumps({"clock":"steady", "window_us":elapsed,
                                                       "sampled_peak_online":peak}),online))

    def test_measured_window_and_online_not_mspt(self):
        self.sample()
        server = self.report()["server"]
        self.assertEqual(server["sampling_status"], "measured")
        self.assertEqual(server["effective_tps"], 20)
        self.assertEqual(server["current_online"], 2)
        self.assertEqual(server["sampled_peak_online"], 3)
        self.assertEqual(server["window_ms"], 5000)
        self.assertEqual(server["sample_count"], 100)
        self.assertEqual(server["sample_age_ms"], 1000)
        self.assertIsNone(server["processing_mspt"])
        self.sample(count=50)
        self.assertEqual(self.report()["server"]["effective_tps"], 10)
        self.sample(count=150)
        self.assertEqual(self.report()["server"]["effective_tps"], 20)

    def test_stale_boundary_does_not_present_live_online_or_tps(self):
        self.sample(stamp=NOW-20000)
        self.assertEqual(self.report()["server"]["sampling_status"], "measured")
        r = self.analytics.report(now_ms=NOW+1)["server"]
        self.assertEqual(r["sampling_status"], "stale")
        self.assertEqual(r["last_effective_tps"],20)
        self.assertIsNone(r["effective_tps"])
        self.assertIsNone(r["current_online"])
        self.assertIsNone(r["sampled_peak_online"])

    def test_newest_invalid_row_does_not_fall_back(self):
        for invalid in [dict(stamp=NOW+1),dict(count=0),dict(count=10001),dict(elapsed=4999999),
                        dict(elapsed=30000001),dict(online=-2),dict(online=100001),dict(peak=1),
                        dict(run_id="legacy"),dict(raw="{}"),dict(raw="[1]"),dict(raw="x"),
                        dict(raw='{"clock":"wall","window_us":5000000,"sampled_peak_online":3}')]:
            with self.subTest(invalid=invalid):
                self.sample()
                self.sample(**invalid)
                r = self.report()["server"]
                self.assertEqual(r["sampling_status"], "invalid")
                self.assertIsNone(r["effective_tps"])
                self.assertIsNone(r["current_online"])

    def test_legacy_and_untrusted_actor_are_not_measurements(self):
        self.sample(cause="legacy")
        self.assertEqual(self.report()["server"]["sampling_status"], "unavailable")
        self.sample()
        with fixture_writer(self.path) as db:
            db.execute("UPDATE events SET actor_kind='player'")
        self.assertEqual(self.report()["server"]["sampling_status"], "unavailable")

    def test_clean_end_sequence_beats_wall_clock_and_new_run_resumes(self):
        self.sample()
        self.sample(cause=END_CAUSE, stamp=NOW-100000) # clock regression still ends run
        self.assertEqual(self.report()["server"]["sampling_status"], "stopped")
        self.assertIsNone(self.report()["server"]["effective_tps"])
        self.sample(run_id="lltick-new")
        self.assertEqual(self.report()["server"]["sampling_status"], "measured")

    def test_unknown_online_is_not_zero(self):
        self.sample(online=-1, peak=-1)
        r = self.report()["server"]
        self.assertEqual(r["effective_tps"],20)
        self.assertIsNone(r["current_online"])
        self.assertIsNone(r["sampled_peak_online"])

    def test_restart_start_marker_hides_previous_crash_sample(self):
        self.sample()
        self.sample(cause=START_CAUSE,run_id="lltick-new")
        self.assertEqual(self.report()["server"]["sampling_status"],"warming_up")
        self.assertIsNone(self.report()["server"]["effective_tps"])
        self.assertEqual(self.analytics.report(now_ms=NOW+20000)["server"]["sampling_status"],"stale")

    def test_recent_sample_authenticated_http(self):
        self.sample(stamp=time.time_ns()//1000000)
        server = CompanionServer(self.path, 0) # not the existing 18767 listener
        thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval":0.01})
        thread.start()
        client = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=5)
        try:
            client.request("GET", "/api/v1/analytics?period=all", headers={"Authorization":"Bearer "+server.viewer_token})
            response = client.getresponse()
            self.assertEqual(response.status,200)
            body = json.loads(response.read())
            self.assertEqual(body["server"]["effective_tps"],20)
            self.assertEqual(body["server"]["current_online"],2)
            self.assertIsNone(body["server"]["processing_mspt"])
        finally:
            client.close()
            server.shutdown();server.server_close();thread.join(3)
