"""Wait for measured samples in an already-authorized NEW scratch BDS only.

Does not launch/control any BDS or access the protected two-player DB. Starts
and closes only its own ephemeral loopback viewer listener. No tokens printed.
"""
import argparse
import http.client
import json
from pathlib import Path
import sqlite3
import subprocess
import shutil
import threading
import time

from .analytics import Analytics
from .server import CompanionServer
from .mspt_acceptance import require


def verify_ui_dom(data, project):
    """No listener/token access: render only this authenticated API response."""
    node = shutil.which("node")
    if node is None:
        raise RuntimeError("Node is required for the actual-assets DOM check")
    rendered = subprocess.run([node, str(project / "tests" / "native_dashboard_ui_check.cjs")],
                              input=json.dumps(data), encoding="utf-8", capture_output=True,
                              timeout=5, check=True)
    return json.loads(rendered.stdout)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--wait-seconds", type=int, default=60, choices=range(1,181))
    parser.add_argument("--require-busy", action="store_true")
    parser.add_argument("--ui-dom", action="store_true",
                        help="Render the measured authenticated report with actual Japanese assets (Node DOM test, not browser)")
    args = parser.parse_args()
    if args.ui_dom and not args.require_busy:
        parser.error("--ui-dom requires --require-busy")
    path = args.database.resolve()
    project = Path(__file__).resolve().parents[1]
    if (not path.is_relative_to(project / ".build" / "isolated-bds")
            or "run-2bbb60dddd2344c3923ae1cbd66fbb98" in path.parts
            or path.name != "bedrock_insight.sqlite3"):
        raise ValueError("Only a separate new isolated telemetry test DB is allowed")
    deadline = time.monotonic() + args.wait_seconds
    while time.monotonic() < deadline:
        try:
            report = Analytics(path, b"verifier-only-not-viewer-auth").report(period="all")
            if (report["server"]["sampling_status"] == "measured"
                    and (not args.require_busy or report["server"]["processing_mspt"] is not None)):
                break
        except (FileNotFoundError, sqlite3.Error, TimeoutError):
            pass
        time.sleep(0.5)
    else:
        raise TimeoutError("No genuine fresh native sample before deadline; never assume TPS/MSPT")
    server = CompanionServer(path, 0)
    thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval":0.01})
    thread.start()
    client = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=5)
    try:
        client.request("GET", "/api/v1/analytics?period=all")
        unauthorized = client.getresponse()
        require(unauthorized.status == 401, "Anonymous API must reject with401")
        unauthorized.read();client.close()
        client = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=5)
        client.request("GET", "/api/v1/analytics?period=all",
                       headers={"Authorization":"Bearer "+server.viewer_token})
        response = client.getresponse()
        require(response.status == 200, "Authenticated API must return200")
        data = json.loads(response.read())
        sample = data["server"]
        require(sample["sampling_status"] == "measured" and sample["effective_tps"] > 0,
                "No fresh running native sample")
        if args.require_busy:
            require(sample["measurement_method"] == "Level::$tick/Highest+Lowest"
                    and sample["processing_mspt"] is not None and sample["processing_mspt"] >= 0
                    and sample["peak_mspt"] is not None and sample["sample_count"] > 0,
                    "Missing genuine busy source and tick count")
        elif sample.get("measurement_method") != "Level::$tick/Highest+Lowest":
            require(sample["effective_tps"] <= 20 and sample["processing_mspt"] is None,
                    "Interval source must not claim processing MSPT")
        ui = None
        if args.ui_dom:
            ui = verify_ui_dom(data, project)
        result = {"verification":"isolated native samples -> SQLite -> authenticated loopback HTTP",
                          "source":str(path),"generated_at_ms":data["generated_at_ms"],
                          "anonymous":unauthorized.status,"authenticated":response.status,"server":sample,
                          "ui_dom":ui,
                          "scope":"Live check only; verify_mspt_capture.ps1 must verify normal stop and durability"}
    finally:
        client.close();server.shutdown();server.server_close();thread.join(3)
    require(not thread.is_alive(), "Own ephemeral HTTP thread did not stop")
    result["listener_stopped_only_own"] = True
    print(json.dumps(result, ensure_ascii=True))


if __name__ == "__main__":
    main()
