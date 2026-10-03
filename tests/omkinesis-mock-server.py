#!/usr/bin/env python3
# Copyright 2026 Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0.
"""One-request mock Kinesis PutRecord endpoint for the config parity tests."""
import http.server
import json
import sys


class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        with open(sys.argv[2], "w", encoding="utf-8") as capture:
            json.dump({"path": self.path, "headers": dict(self.headers),
                       "body": json.loads(body)}, capture)
        self.send_response(200)
        self.send_header("Content-Type", "application/x-amz-json-1.1")
        self.end_headers()
        self.wfile.write(b'{"SequenceNumber":"1","ShardId":"shardId-000000000000"}')

    def log_message(self, format, *args):
        pass


server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
with open(sys.argv[1], "w", encoding="ascii") as port_file:
    port_file.write(str(server.server_address[1]))
server.handle_request()
server.server_close()
