#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Regression test for omelasticsearch bulk maxbytes boundaries. The local HTTP
# endpoint records every bulk body. The oracle proves that fixed bulk metadata
# is included in the size estimate: two records must be split at the configured
# boundary. It also proves that an oversized first record produces one request,
# not an empty flush followed by that record. The helper writes its port only
# after bind and is terminated during test cleanup.
. ${srcdir:=.}/diag.sh init
require_plugin omelasticsearch
check_command_available python3

PORT_FILE="$RSYSLOG_DYNNAME.esfake.port"
REQUESTS_FILE="$RSYSLOG_DYNNAME.esfake.requests"

test_error_exit_handler() {
	if [ -n "${SERVER_PID:-}" ]; then
		kill "$SERVER_PID" 2>/dev/null || true
	fi
}

python3 - "$PORT_FILE" "$REQUESTS_FILE" <<'PY' &
import base64
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

port_file, requests_file = sys.argv[1:3]


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def send_json(self, payload):
        data = json.dumps(payload).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        self.send_json({
            "version": {
                "number": "7.17.0",
                "distribution": "elasticsearch",
            }
        })

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        with open(requests_file, "a", encoding="ascii") as fh:
            fh.write(base64.b64encode(body).decode("ascii") + "\n")
        operations = len(body.splitlines()) // 2
        operation = "index"
        if body:
            operation = next(iter(json.loads(body.splitlines()[0])))
        self.send_json({
            "errors": False,
            "items": [{operation: {"status": 201}} for _ in range(operations)],
        })


server = HTTPServer(("127.0.0.1", 0), Handler)
with open(port_file, "w", encoding="ascii") as fh:
    fh.write(f"{server.server_port}\n")
server.serve_forever()
PY
SERVER_PID=$!

wait_file_exists "$PORT_FILE" "fake Elasticsearch helper did not publish a port"
ES_PORT=$(cat "$PORT_FILE")

generate_conf
add_conf '
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
template(name="msgTpl" type="string" string="{\"msg\":\"x\"}")

if $msg contains "msgnum:" then {
action(type="omelasticsearch"
       server="127.0.0.1"
       serverport="'$ES_PORT'"
       bulkmode="on"
       searchIndex="idx"
       searchType="legacy"
       writeoperation="create"
       bulkid="id"
       esversion.major="7"
       maxbytes="109"
       template="msgTpl")
}
'

startup
injectmsg 0 2
shutdown_when_empty
wait_shutdown

python3 - "$REQUESTS_FILE" <<'PY'
import base64
import sys
from pathlib import Path

bodies = [base64.b64decode(line) for line in Path(sys.argv[1]).read_text().splitlines()]
if len(bodies) != 2:
    raise SystemExit(f"expected two boundary-limited bulk requests, got {len(bodies)}")
if any(not body or len(body) > 109 for body in bodies):
    raise SystemExit("boundary-limited bulk request was empty or exceeded 109 bytes")
PY
if [ $? -ne 0 ]; then
	error_exit 1
fi

: > "$REQUESTS_FILE"
generate_conf
add_conf '
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
template(name="msgTpl" type="string" string="{\"msg\":\"x\"}")

if $msg contains "msgnum:" then {
action(type="omelasticsearch"
       server="127.0.0.1"
       serverport="'$ES_PORT'"
       bulkmode="on"
       searchIndex="idx"
       searchType="legacy"
       writeoperation="create"
       bulkid="id"
       esversion.major="7"
       maxbytes="48"
       template="msgTpl")
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

python3 - "$REQUESTS_FILE" <<'PY'
import base64
import sys
from pathlib import Path

bodies = [base64.b64decode(line) for line in Path(sys.argv[1]).read_text().splitlines()]
if len(bodies) != 1 or not bodies[0]:
    raise SystemExit("oversized first record caused an empty bulk request")
PY
if [ $? -ne 0 ]; then
	error_exit 1
fi

kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
exit_test
