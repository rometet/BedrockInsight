"""Run: python -m companion.server --database <BedrockInsight SQLite path>."""
from __future__ import annotations

import argparse
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, HTTPServer
import io
import json
from pathlib import Path
import secrets
import socket
import sqlite3
import time
from urllib.parse import parse_qs, urlsplit

from .analytics import Analytics
from .stats_view import report as stats_report
from .stats_catalog import BASE_TYPE_LABELS
from .type_labels import load_type_labels

ASSETS = Path(__file__).parent / "web"


class CompanionServer(HTTPServer):
    allow_reuse_address = False
    request_queue_size = 8

    def __init__(self, database, port=18767, type_labels_path=None):
        # Load before binding; an invalid optional file cannot leave a listener.
        self.type_labels=(load_type_labels(type_labels_path,BASE_TYPE_LABELS)
                          if type_labels_path is not None else BASE_TYPE_LABELS)
        self.viewer_token = secrets.token_urlsafe(32)
        self.token_hash = hashlib.sha256(self.viewer_token.encode("ascii")).digest()
        self.token_deadline = time.monotonic() + 8 * 3600
        self.analytics = Analytics(Path(database), secrets.token_bytes(32))
        super().__init__(("127.0.0.1", port), Handler)

    def server_bind(self):
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

    def get_request(self):
        client, address = super().get_request()
        client.settimeout(2)
        return client, address


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    server_version = "BedrockInsightCompanion"

    def log_message(self, _format, *_args):
        pass  # Never log tokens, names, queries or request headers.

    def handle(self):
        # Bound TOTAL header time/bytes, not just each recv. No keep-alive or
        # thread-per-connection amplification. Never read a request body.
        deadline = time.monotonic() + 2
        data = bytearray()
        try:
            while b"\r\n\r\n" not in data:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return
                self.connection.settimeout(remaining)
                part = self.connection.recv(min(1024, 8193-len(data)))
                if not part:
                    return
                data.extend(part)
                if len(data) > 8192:
                    self.request_version, self.requestline, self.command = "HTTP/1.0", "", None
                    self.reply(431, {"error": "request too large"})
                    return
            original = self.rfile
            self.rfile = io.BytesIO(data)
            original.close()
            self.handle_one_request()
        except (ConnectionError, TimeoutError, OSError):
            self.close_connection = True

    def reply(self, status, body, content_type="application/json; charset=utf-8"):
        if not isinstance(body, bytes):
            body = json.dumps(body, ensure_ascii=True, allow_nan=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Frame-Options", "DENY")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Content-Security-Policy", "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def do_GET(self):
        try:
            self.get()
        except (ConnectionError, TimeoutError, BrokenPipeError):
            self.close_connection = True
        except Exception:
            self.reply(503, {"error": "データの読み取りが一時的にできません。BDSは変更していません。"})

    def get(self):
        hosts = {f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"}
        host_values = self.headers.get_all("Host", [])
        origin_values = self.headers.get_all("Origin", [])
        if len(host_values) != 1 or host_values[0] not in hosts:
            return self.reply(403, {"error": "loopback Host only"})
        if origin_values and (len(origin_values) != 1 or origin_values[0] != "http://" + host_values[0]):
            return self.reply(403, {"error": "same-origin only"})
        if self.headers.get("Sec-Fetch-Site") == "cross-site":
            return self.reply(403, {"error": "cross-site denied"})
        if len(self.path) > 2048 or sum(len(k)+len(v) for k, v in self.headers.items()) > 8192:
            return self.reply(431, {"error": "request too large"})
        if self.headers.get("Transfer-Encoding") or self.headers.get("Content-Length", "0") != "0":
            return self.reply(400, {"error": "GET body denied"})
        parts = urlsplit(self.path)
        if parts.scheme or parts.netloc or parts.fragment:
            return self.reply(400, {"error": "relative target only"})
        assets = {"/": ("index.html", "text/html; charset=utf-8"),
                  "/actions": ("actions.html", "text/html; charset=utf-8"),
                  "/actions.js": ("actions.js", "text/javascript; charset=utf-8"),
                  "/stats": ("stats.html", "text/html; charset=utf-8"),
                  "/stats.js": ("stats.js", "text/javascript; charset=utf-8"),
                  "/stats.css": ("stats.css", "text/css; charset=utf-8"),
                  "/app.js": ("app.js", "text/javascript; charset=utf-8"),
                  "/styles.css": ("styles.css", "text/css; charset=utf-8")}
        if parts.path in assets:
            if parts.query:
                return self.reply(400, {"error": "asset query denied"})
            name, content_type = assets[parts.path]
            return self.reply(200, (ASSETS / name).read_bytes(), content_type)
        if parts.path not in ("/api/v1/analytics", "/api/v1/stats"):
            return self.reply(404, {"error": "not found"})
        auth = self.headers.get_all("Authorization", [])
        value = auth[0] if len(auth) == 1 else ""
        valid = value.startswith("Bearer ") and len(value) < 256
        supplied = value[7:] if valid else ""
        if (not valid or time.monotonic() >= self.server.token_deadline or
                not hmac.compare_digest(hashlib.sha256(supplied.encode()).digest(), self.server.token_hash)):
            return self.reply(401, {"error": "閲覧トークンが必要です（再起動後は新しいトークン）"})
        try:
            stats = parts.path == "/api/v1/stats"
            query = parse_qs(parts.query, keep_blank_values=True, strict_parsing=True,
                             max_num_fields=12 if stats else 8) if parts.query else {}
            allowed = ({"category", "key", "period", "date", "player", "search", "type", "page", "rank_page", "limit", "players_page"}
                       if stats else {"period", "date", "metric", "player", "page", "limit", "players_page"})
            if set(query) - allowed or any(len(v) != 1 for v in query.values()):
                raise ValueError("不明または重複したパラメータ")
            args = {k: v[0] for k, v in query.items()}
            for key in (("page", "rank_page", "limit", "players_page") if stats else ("page", "limit", "players_page")):
                if key in args:
                    if not args[key].isascii() or not args[key].isdecimal() or len(args[key]) > 5:
                        raise ValueError("ページは範囲内の整数です")
                    args[key] = int(args[key])
            report = stats_report(self.server.analytics, type_labels=self.server.type_labels, **args) if stats else self.server.analytics.report(**args)
        except (ValueError, OverflowError) as error:
            return self.reply(400, {"error": str(error)})
        except LookupError:
            return self.reply(404, {"error": "プレイヤーが見つかりません"})
        except (sqlite3.Error, TimeoutError):
            return self.reply(503, {"error": "読み取り待機または対応外DB。サーバーの設定は変更していません。"})
        self.reply(200, report)

    def do_POST(self):
        self.reply(405, {"error": "read-only: GET only"})

    do_PUT = do_DELETE = do_PATCH = do_OPTIONS = do_POST


def main():
    parser = argparse.ArgumentParser(description="BedrockInsight localhost-only read-only companion (NOT in-game HTTP)")
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--port", default=18767, type=int)
    parser.add_argument("--type-labels", type=Path, help="Optional bounded Japanese type-label override JSON (read once before bind)")
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("port must be 1024..65535; bind address is always 127.0.0.1")
    # Validate data availability before starting a listener; no default token/config file.
    try:
        server = CompanionServer(args.database, args.port, args.type_labels)
    except (OSError,ValueError) as error:
        parser.error(str(error))
    try:
        server.analytics.report(period="all")
        print(f"BedrockInsight COMPANION (read-only): http://127.0.0.1:{server.server_port}/", flush=True)
        print("Viewer token (memory only, expires in 8h; paste into this localhost page): " + server.viewer_token, flush=True)
        print("Ctrl+C stops ONLY this companion. Native BDS/HTTP/rollback are unchanged.", flush=True)
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
