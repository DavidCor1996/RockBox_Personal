#!/usr/bin/env python3
"""Tiny RockPod music sharing relay.

Run this on a public host or behind an HTTPS tunnel. It stores share cards by
pair code and exposes only the endpoints the RockPod app uses:

  POST /v1/share
  GET  /v1/inbox?pair_code=...
"""

from __future__ import annotations

import argparse
import json
import os
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse


class RelayStore:
    def __init__(self, path):
        self.path = os.path.abspath(path)
        self._lock = threading.RLock()

    def _load(self):
        if not os.path.exists(self.path):
            return {}
        try:
            with open(self.path, "r") as handle:
                data = json.load(handle)
        except (OSError, json.JSONDecodeError):
            return {}
        return data if isinstance(data, dict) else {}

    def _save(self, data):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "w") as handle:
            json.dump(data, handle, indent=2, sort_keys=True)

    def add(self, pair_code, message):
        code = str(pair_code or "").strip()
        if not code:
            raise ValueError("pair_code is required")
        if not isinstance(message, dict):
            raise ValueError("message must be an object")
        with self._lock:
            data = self._load()
            bucket = data.setdefault(code, [])
            message_id = str(message.get("id") or "")
            if message_id and any(str(item.get("id") or "") == message_id for item in bucket):
                return
            bucket.append(message)
            data[code] = bucket[-500:]
            self._save(data)

    def list(self, pair_code):
        code = str(pair_code or "").strip()
        if not code:
            return []
        with self._lock:
            return list(self._load().get(code) or [])


def make_handler(store):
    class Handler(BaseHTTPRequestHandler):
        server_version = "RockPodMusicShareRelay/1.0"

        def _send_json(self, status, payload):
            body = json.dumps(payload).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):  # noqa: N802 - stdlib API
            parsed = urlparse(self.path)
            if parsed.path == "/health":
                self._send_json(200, {"ok": True})
                return
            if parsed.path != "/v1/inbox":
                self._send_json(404, {"error": "not_found"})
                return
            query = parse_qs(parsed.query)
            pair_code = (query.get("pair_code") or [""])[0]
            self._send_json(200, {"messages": store.list(pair_code)})

        def do_POST(self):  # noqa: N802 - stdlib API
            if urlparse(self.path).path != "/v1/share":
                self._send_json(404, {"error": "not_found"})
                return
            try:
                length = int(self.headers.get("Content-Length") or "0")
            except ValueError:
                length = 0
            try:
                payload = json.loads(self.rfile.read(length).decode("utf-8"))
                store.add(payload.get("pair_code"), payload.get("message"))
            except (OSError, json.JSONDecodeError, ValueError) as exc:
                self._send_json(400, {"error": str(exc)})
                return
            self._send_json(200, {"ok": True})

        def log_message(self, format, *args):  # noqa: A003 - stdlib API
            return

    return Handler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument(
        "--store",
        default=os.environ.get("ROCKPOD_SHARE_STORE", os.path.expanduser("~/.rockpod/share-relay.json")),
    )
    args = parser.parse_args()
    store = RelayStore(args.store)
    server = ThreadingHTTPServer((args.host, args.port), make_handler(store))
    print(f"RockPod music share relay listening on http://{args.host}:{args.port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
