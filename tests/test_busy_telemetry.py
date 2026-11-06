import http.client
import json
import threading
import time
from pathlib import Path

from test_analytics_companion import Fixture, fixture_writer, NOW
from companion.server import CompanionServer
from companion.telemetry import BUSY_CAUSE, BUSY_START, BUSY_END, BUSY_INVALID
from companion.verify_ticks import verify_ui_dom


class BusyTelemetryTests(Fixture):
    def setUp(self):
        super().setUp()
        with fixture_writer(self.path) as db:
            for name, kind in (("session_id","TEXT"),("online_players","INTEGER")):
                db.execute(f"ALTER TABLE events ADD COLUMN {name} {kind}")

    def sample(self, *, ticks=100, wall=5_000_000_000, busy=200_000_000,
               peak=2_000_000, frames=100, stamp=NOW-1000, cause=BUSY_CAUSE, raw=None):
        data={"clock":"steady","hook":"Level::$tick/Highest+Lowest","wall_ns":wall,
              "busy_ns":busy,"max_inner_ns":peak,"frames":frames,"sampled_peak_online":3}
        with fixture_writer(self.path) as db:
            db.execute("""INSERT INTO events(occurred_at_ms,event_kind,actor_kind,cause,
                session_id,amount,details_json,online_players)
                VALUES(?,'server_tick','system',?,'lltick-busy-fixture',?,?,2)""",
                (stamp,cause,ticks,json.dumps(data) if raw is None else raw))

    def test_idle_excludes_sleep_lag_warp_and_freeze(self):
        for values, tps, mspt in [({},20,2),
            ({"ticks":50,"busy":4_000_000_000,"peak":80_000_000,"frames":50},10,80),
            ({"ticks":300,"busy":600_000_000},60,2),
            ({"ticks":0,"busy":10_000_000,"peak":0},0,None)]:
            self.sample(**values)
            r=self.report()["server"]
            self.assertEqual(r["effective_tps"],tps)
            self.assertEqual(r["processing_mspt"],mspt)
            self.assertEqual(r["measurement_method"],"Level::$tick/Highest+Lowest")
            self.assertEqual(r["window_ms"],5000)
            self.assertEqual(r["current_online"],2)
            self.assertEqual(r["last_sample_ms"],NOW-1000)
            self.assertIsNone(r["p95_mspt"])
        self.assertEqual(r["sampling_status"],"frozen")
        self.assertIsNone(r["peak_mspt"])

    def test_stale_end_invalid_start_never_fall_back_to_old_sample(self):
        self.sample(stamp=NOW-20000)
        self.assertEqual(self.report()["server"]["processing_mspt"],2)
        r=self.analytics.report(now_ms=NOW+1)["server"]
        self.assertEqual(r["sampling_status"],"stale")
        self.assertIsNone(r["processing_mspt"])
        self.sample(cause=BUSY_START)
        self.assertEqual(self.report()["server"]["sampling_status"],"warming_up")
        self.assertIsNone(self.report()["server"]["processing_mspt"])
        self.sample(cause=BUSY_START,stamp=NOW-21000)
        self.assertEqual(self.report()["server"]["sampling_status"],"stale")
        for marker, expected in [(BUSY_END,"stopped"),(BUSY_INVALID,"invalid"),
                                 ("ll26.60.busytick.v2","invalid")]:
            self.sample(cause=marker)
            r=self.report()["server"]
            self.assertEqual(r["sampling_status"],expected)
            for key in ("processing_mspt","effective_tps","current_online","peak_mspt"):
                self.assertIsNone(r[key])

    def test_malformed_clock_period_overflow_future_and_missing_data(self):
        cases=[{"wall":4_999_999_999},{"wall":30_000_000_001},{"busy":-1},
               {"busy":5_000_000_001},{"ticks":-1},{"ticks":1_000_001},
               {"frames":0},{"frames":1_000_001},{"peak":200_000_001},
               {"ticks":0,"peak":1},{"stamp":NOW+1},{"raw":"{}"},
               {"raw":"[]"},{"raw":"null"},{"raw":"x"},
               {"raw":json.dumps({"clock":"wall","hook":"Level::$tick/Highest+Lowest",
                    "wall_ns":5_000_000_000,"busy_ns":200_000_000,"max_inner_ns":2_000_000,
                    "frames":100,"sampled_peak_online":3})}]
        for values in cases:
            with self.subTest(values=values):
                self.sample()
                self.sample(**values)
                r=self.report()["server"]
                self.assertEqual(r["sampling_status"],"invalid")
                self.assertIsNone(r["processing_mspt"])

    def test_busy_lifecycle_authoritative_over_interval_and_legacy(self):
        self.sample(cause=BUSY_END)
        self.sample(cause="ll26.60.interval.v1",raw=json.dumps({"clock":"steady","window_us":5000000,"sampled_peak_online":3}))
        with fixture_writer(self.path) as db:
            db.execute("INSERT INTO server_tick_samples VALUES(99,?,50,2)",(NOW-1,))
        r=self.report()["server"]
        self.assertEqual(r["sampling_status"],"stopped")
        self.assertIsNone(r["processing_mspt"])
        self.assertIsNone(r["effective_tps"])

    def test_fresh_persisted_sample_authenticated_http_and_ui_contract(self):
        self.sample(stamp=time.time_ns()//1000000)
        server=CompanionServer(self.path,0)
        thread=threading.Thread(target=server.serve_forever,kwargs={"poll_interval":0.01})
        thread.start()
        client=http.client.HTTPConnection("127.0.0.1",server.server_port,timeout=5)
        try:
            client.request("GET","/api/v1/analytics?period=all",headers={"Authorization":"Bearer "+server.viewer_token})
            response=client.getresponse()
            self.assertEqual(response.status,200)
            data=json.loads(response.read())
            self.assertEqual(data["server"]["processing_mspt"],2)
            self.assertEqual(data["server"]["peak_mspt"],2)
            self.assertEqual(data["server"]["effective_tps"],20)
            rendered=verify_ui_dom(data,Path(__file__).resolve().parents[1])
            self.assertEqual(rendered["mspt"],"2")
            self.assertEqual(rendered["tps"],"20")
            data["server"]["current_online"]=None
            self.assertEqual(verify_ui_dom(data,Path(__file__).resolve().parents[1])["online"],"未計測")
        finally:
            client.close();server.shutdown();server.server_close();thread.join(3)
