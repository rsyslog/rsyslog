#!/usr/bin/env python3
"""Simple ClickHouse HTTP mock used by omclickhouse tests.

The server accepts POST requests and stores non-health-check payloads into
numbered files under a sink directory so the shell tests can assert on the
exact SQL statements that omclickhouse emits. Health checks ("SELECT 1")
receive a ``200 OK`` response with a small body.
"""
from __future__ import annotations

import argparse
import signal
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional


class RequestStore:
    """Persist incoming SQL payloads into numbered files."""

    def __init__(self, sink_dir: Path) -> None:
        self._sink_dir = sink_dir
        self._sink_dir.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._counter = 0

    def store(self, payload: str) -> Path:
        with self._lock:
            self._counter += 1
            path = self._sink_dir / f"request_{self._counter:04d}.sql"
        path.write_text(payload, encoding="utf-8")
        return path


class ClickHouseMockHandler(BaseHTTPRequestHandler):
    """Minimal handler that understands ClickHouse-style POSTs."""

    protocol_version = "HTTP/1.1"

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler interface
        length_header = self.headers.get("Content-Length", "0")
        try:
            length = int(length_header)
        except ValueError:
            length = 0
        payload_bytes = self.rfile.read(length)
        payload = payload_bytes.decode("utf-8", errors="replace")
        body = "OK\n"

        if payload.strip().upper() == "SELECT 1":
            body = "1\n"
        else:
            STORE.store(payload)

        encoded = body.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def log_message(self, format: str, *args: object) -> None:  # noqa: A003 - matches BaseHTTPRequestHandler signature
        """Suppress default stderr logging to keep test output clean."""
        return


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="Interface to bind (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, required=True, help="Port to listen on")
    parser.add_argument("--sink-dir", required=True, help="Directory to store POST payloads")
    parser.add_argument(
        "--ready-file",
        required=True,
        help="File that will be created once the server is ready; contains the bound port",
    )
    return parser.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    sink_dir = Path(args.sink_dir)
    global STORE
    STORE = RequestStore(sink_dir)

    server = ThreadingHTTPServer((args.host, args.port), ClickHouseMockHandler)
    server.daemon_threads = True

    def shutdown_handler(signum: int, _frame: object) -> None:
        # Shut down gracefully when receiving SIGTERM/SIGINT so the tests do not hang.
        server.shutdown()

    signal.signal(signal.SIGTERM, shutdown_handler)
    signal.signal(signal.SIGINT, shutdown_handler)

    ready_path = Path(args.ready_file)
    ready_path.parent.mkdir(parents=True, exist_ok=True)
    ready_path.write_text(str(server.server_port), encoding="utf-8")

    try:
        server.serve_forever()
    finally:
        server.server_close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
