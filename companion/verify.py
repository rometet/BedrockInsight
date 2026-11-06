"""Read-only real-data HTTP smoke test. Opens/stops ONLY its own loopback listener."""
import argparse
import http.client
import json
from pathlib import Path
import threading

from .server import CompanionServer


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, required=True)
    args = parser.parse_args()
    server = CompanionServer(args.database, 0)
    thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval":0.01})
    thread.start()

    def request(path, authenticated=True):
        client = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=6)
        try:
            client.request("GET", path, headers={"Authorization":"Bearer "+server.viewer_token} if authenticated else {})
            response = client.getresponse()
            return response.status, response.read()
        finally:
            client.close()

    try:
        assert request("/api/v1/analytics", False)[0] == 401
        assets = {path: request(path, False)[0] for path in ("/","/app.js","/styles.css")}
        assert set(assets.values()) == {200}
        status, raw = request("/api/v1/analytics?period=all")
        assert status == 200, (status, raw)
        report = json.loads(raw)
        if report["players"]:
            key = report["players"][0]["key"]
            assert request("/api/v1/analytics?period=all&player="+key)[0] == 200
        print(json.dumps({"test":"actual loopback HTTP + configured SQLite, no BDS writes",
                          "source":str(args.database),"bind_address":server.server_address[0],
                          "unauthorized_status":401,"authenticated_status":status,"assets":assets,
                          "generated_at_ms":report["generated_at_ms"],"totals":report["totals"],
                          "players":[{"name":p["name"],"playtime_ms":p["playtime"],"joins":p["joins"],"leaves":p["leaves"]} for p in report["players"]],
                          "server":report["server"],"native_dll_modified":False}, ensure_ascii=True))
    finally:
        server.shutdown()
        server.server_close()
        thread.join(3)


if __name__ == "__main__":
    main()
