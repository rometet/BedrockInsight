"""Read-only evidence consistency gate; never launches/stops BDS or fabricates samples.

Fixtures can prove this checker, not genuine native capture. Runtime evidence
must come from the authorized isolated runner, with a live verify_ticks report.
"""
import argparse
from datetime import datetime
import json
import math
from pathlib import Path
import sqlite3
import time

from .telemetry import BUSY_CAUSE, BUSY_START, BUSY_END, parse_busy_observation


def require(condition, message):
    # Not assert: acceptance gates must remain active under python -O.
    if not condition:
        raise ValueError(message)


def utc_ms(value):
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    require(parsed.utcoffset() is not None, "Runtime timestamp needs timezone")
    return int(parsed.timestamp()*1000)


def finite_number(value):
    return type(value) in (int, float) and math.isfinite(value)


def check_evidence(runtime, live, rows, source):
    """Pure bounded validation. rows are event_id followed by parse_busy's tuple."""
    for flag in ("native_loaded", "native_enabled", "server_started", "stop_command_issued",
                 "bds_load_verified", "loader_config_migrated_with_disabled_overrides"):
        require(runtime.get(flag) is True, "Missing actual runtime gate: "+flag)
    require(type(runtime.get("exit_code")) is int and runtime["exit_code"] == 0
            and runtime.get("forced_termination") is False and runtime.get("error") is None,
            "Need graceful exit0 without forced termination/error")
    require(runtime.get("startup_outcome", {}).get("stop_reason") == "ready_hold_completed"
            and runtime.get("capture_access", {}).get("mode") == "empty-smoke",
            "Expected owned empty-allowlist smoke completion, not a self-crash")
    require(runtime.get("preflight", {}).get("candidate_policy_passed") is True,
            "Missing full isolated preflight")
    start, end = utc_ms(runtime["started_utc"]), utc_ms(runtime["finished_utc"])
    require(0 <= start < end, "Invalid runtime time bounds")
    require(4 <= len(rows) <= 1000, "Need start, at least two busy windows, end; bounded1000 rows")
    require(rows[0][2] == BUSY_START and rows[-1][2] == BUSY_END, "Missing ordered start/end")
    activation = rows[0][3]
    samples, previous_id, previous_stamp = [], -1, start
    for index, row in enumerate(rows):
        event_id, stamp, cause, run_id, *_ = row
        require(type(event_id) is int and event_id > previous_id, "Invalid durable event order")
        require(type(stamp) is int and previous_stamp <= stamp <= end,
                "Window outside runtime or wall clock went backwards")
        require(run_id == activation, "Mixed/restarted activations are not one successful smoke run")
        if index not in (0, len(rows)-1):
            require(cause == BUSY_CAUSE, "Invalid/unsupported/interrupted busy stream")
            sample = parse_busy_observation(row[1:], stamp)
            require(sample["sampling_status"] == "measured" and sample["sample_count"] > 0,
                    "No actual busy ticks in a valid >=5s window")
            samples.append(sample)
        else:
            require(parse_busy_observation(row[1:], stamp)["sampling_status"] in ("warming_up", "stopped"),
                    "Invalid lifecycle marker")
        previous_id, previous_stamp = event_id, stamp
    require(len(samples) >= 2, "Two genuine busy windows required")
    require(Path(live.get("source", "")).resolve() == Path(source).resolve(), "Live report used another DB")
    require(type(live.get("anonymous")) is int and live["anonymous"] == 401
            and type(live.get("authenticated")) is int and live["authenticated"] == 200
            and live.get("listener_stopped_only_own") is True, "Missing authenticated live HTTP proof")
    generated = live.get("generated_at_ms")
    require(type(generated) is int and start <= generated <= end, "API capture outside running test")
    server = live.get("server", {})
    matches = [s for s in samples if s["last_sample_ms"] == server.get("last_sample_ms")]
    require(len(matches) == 1, "API sample not found uniquely in durable busy rows")
    stored = matches[0]
    require(0 <= generated-stored["last_sample_ms"] <= 20000, "API captured stale/future data")
    require(server.get("sampling_status") == "measured"
            and server.get("measurement_method") == "Level::$tick/Highest+Lowest", "Not measured busy telemetry")
    for key in ("processing_mspt", "effective_tps", "peak_mspt", "sample_count", "window_ms", "frame_count"):
        require(finite_number(server.get(key)) and server[key] == stored[key], "API/durable mismatch: "+key)
    require(type(server.get("current_online")) is type(stored["current_online"])
            and server.get("current_online") == stored["current_online"], "API online mismatch")
    ui = live.get("ui_dom") or {}
    for field, api_key in (("mspt", "processing_mspt"), ("tps", "effective_tps")):
        try:
            rendered = float(ui[field])
        except (KeyError, TypeError, ValueError):
            raise ValueError("Missing numeric Japanese DOM rendering: "+field) from None
        require(math.isfinite(rendered) and rendered == server[api_key], "DOM/API mismatch: "+field)
    require(ui.get("verification") == "actual Japanese assets DOM, not browser"
            and "Level::$tick" in ui.get("sampling", ""), "Missing honest Japanese DOM proof")
    require(ui.get("online") == ("未計測" if stored["current_online"] is None else str(stored["current_online"])),
            "DOM online mismatch")
    return {"busy_windows":len(samples), "actual_ticks":sum(s["sample_count"] for s in samples),
            "api_sample_ms":stored["last_sample_ms"], "processing_mspt":stored["processing_mspt"],
            "effective_tps":stored["effective_tps"], "normal_stop":True,
            "sqlite_api_dom_consistent":True, "browser_verified":False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--live-report", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    path = args.snapshot.resolve(strict=True)
    require(path.is_relative_to(project/".build"/"mspt-evidence"), "Only an owned closed-run snapshot")
    require(path.stat().st_size <= 256*1024*1024, "Snapshot exceeds256MiB budget")
    require(not Path(str(path)+"-wal").exists() and not Path(str(path)+"-shm").exists(), "Snapshot must be closed")
    # Wrapper copies a normal-stopped closed DB under a read-only source lock.
    # immutable is ONLY for this private closed copy, never the original/live DB.
    db = sqlite3.connect(path.as_uri()+"?mode=ro&immutable=1", uri=True)
    try:
        deadline = time.monotonic()+2
        db.set_progress_handler(lambda: int(time.monotonic()>deadline), 1000)
        db.execute("PRAGMA query_only=ON")
        require(db.execute("PRAGMA user_version").fetchone() == (4,), "Schema must remain v4")
        require(db.execute("PRAGMA integrity_check").fetchall() == [("ok",)], "SQLite integrity failure")
        require(db.execute("PRAGMA foreign_key_check").fetchone() is None, "SQLite foreign key failure")
        rows = db.execute("""SELECT event_id,occurred_at_ms,cause,session_id,amount,details_json,online_players
            FROM events WHERE event_kind='server_tick' AND actor_kind='system'
            AND cause LIKE 'll26.60.busytick.%' ORDER BY event_id LIMIT 1001""").fetchall()
        runtime = json.loads(args.runtime.read_text(encoding="utf-8-sig"))
        live = json.loads(args.live_report.read_text(encoding="utf-8-sig"))
        result = check_evidence(runtime, live, rows, args.source)
        print(json.dumps({"verification":"isolated runtime/closed SQLite/live HTTP/Japanese DOM consistency",
                          "result":result, "limits":"Evidence consistency, not independent attestation against edited files or a browser screenshot."}))
    finally:
        db.close()


if __name__ == "__main__":
    main()
