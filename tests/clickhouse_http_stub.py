#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
"""Local HTTP stand-in for omclickhouse tests.

Each POST is appended to the record file as one JSON object. A body of
exactly ``SELECT 1`` is a health check and always returns 200. An insert is
rejected while --reject-inserts still has failures left, and every insert
whose body contains the --reject-match text is rejected. A rejected insert
gets the --reject-status code (400 by default) and the --reject-body text
(``BAD REQUEST`` by default), and its record has ``rejected`` set. With
--reject-code, the reply also carries that value in X-ClickHouse-Exception-Code,
as ClickHouse sends it. --outage-match answers the first --outage-count inserts
whose body contains that text, before any other rule, with --fail-status (503
by default) and no code, as a server that is down or restarting while that
batch is sent. --fail-match answers the first
--fail-match-count inserts whose body contains that text the same way.
Other inserts return 200. Each response closes the connection so the
single-threaded server can accept the next one. The listen port is written
only after the socket is bound.
"""

import argparse
import json
import os
from http.server import BaseHTTPRequestHandler, HTTPServer


class Recorder(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        return

    def _send(self, status, payload, exception_code=None):
        body = payload if isinstance(payload, bytes) else payload.encode("utf-8")
        # One request per connection. libcurl keeps the insert connection open,
        # and this single-threaded server would otherwise never accept the
        # health check on the next connection.
        self.close_connection = True
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        if exception_code is not None:
            self.send_header("X-ClickHouse-Exception-Code", str(exception_code))
        self.end_headers()
        self.wfile.write(body)

    def _record(self, kind, status, body, rejected=False):
        rec = {
            "kind": kind,
            "status": status,
            "rejected": rejected,
            "path": self.path,
            "content_type": self.headers.get("Content-Type", ""),
            "content_encoding": self.headers.get("Content-Encoding", ""),
            "body": body,
        }
        fp = self.server.record_fp
        fp.write(json.dumps(rec, ensure_ascii=True) + "\n")
        fp.flush()
        os.fsync(fp.fileno())

    def _reject(self, text):
        status = self.server.reject_status
        self._record("insert", status, text, rejected=True)
        self._send(status, self.server.reject_body, self.server.reject_code)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0") or 0)
        raw = self.rfile.read(length)
        text = raw.decode("utf-8", "replace")
        if text == "SELECT 1":
            self._record("health", 200, text)
            self._send(200, b"")
            return
        omatch = self.server.outage_match
        if omatch and omatch in text and self.server.outaged < self.server.outage_count:
            self.server.outaged += 1
            self._record("insert", self.server.fail_status, text, rejected=True)
            self._send(self.server.fail_status, b"Service Unavailable")
            return
        match = self.server.reject_match
        if match and match in text:
            self._reject(text)
            return
        fmatch = self.server.fail_match
        if fmatch and fmatch in text and self.server.fail_matched < self.server.fail_match_count:
            self.server.fail_matched += 1
            self._record("insert", self.server.fail_status, text, rejected=True)
            self._send(self.server.fail_status, b"Service Unavailable")
            return
        if self.server.rejected < self.server.reject_inserts:
            self.server.rejected += 1
            self._reject(text)
            return
        self._record("insert", 200, text)
        self._send(200, b"")


def main():
    parser = argparse.ArgumentParser(description="omclickhouse HTTP stub")
    parser.add_argument("--interface", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--port-file", required=True)
    parser.add_argument("--record", required=True)
    parser.add_argument("--reject-inserts", type=int, default=0)
    parser.add_argument("--reject-status", type=int, default=400)
    parser.add_argument("--reject-match", default="")
    parser.add_argument("--reject-body", default="BAD REQUEST")
    parser.add_argument("--reject-code", type=int, default=None)
    parser.add_argument("--outage-match", default="")
    parser.add_argument("--outage-count", type=int, default=0)
    parser.add_argument("--fail-status", type=int, default=503)
    parser.add_argument("--fail-match", default="")
    parser.add_argument("--fail-match-count", type=int, default=1)
    args = parser.parse_args()

    server = HTTPServer((args.interface, args.port), Recorder)
    server.reject_inserts = args.reject_inserts
    server.rejected = 0
    server.reject_status = args.reject_status
    server.reject_match = args.reject_match
    server.reject_body = args.reject_body.encode("utf-8")
    server.reject_code = args.reject_code
    server.outage_match = args.outage_match
    server.outage_count = args.outage_count
    server.fail_status = args.fail_status
    server.outaged = 0
    server.fail_match = args.fail_match
    server.fail_match_count = args.fail_match_count
    server.fail_matched = 0
    server.record_fp = open(args.record, "w", encoding="utf-8")
    port = server.server_address[1]
    with open(args.port_file, "w", encoding="utf-8") as port_fp:
        port_fp.write("{0}\n".format(port))
        port_fp.flush()
        os.fsync(port_fp.fileno())
    try:
        server.serve_forever()
    finally:
        server.record_fp.close()


if __name__ == "__main__":
    main()
