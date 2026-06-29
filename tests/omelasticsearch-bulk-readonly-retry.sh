#!/bin/bash
# Regression test for Elasticsearch read-only-index bulk responses. A fake
# Elasticsearch endpoint returns HTTP 200 with per-item status 403 and
# cluster_block_exception, matching a read-only index. Success is proven by a
# second POST to the fake endpoint before the normal startup/shutdown timeout.
# The first POST returns only retryable per-item errors; the second succeeds, so
# the action must suspend/retry the batch instead of treating the bulk item
# failure as a handled permanent data error. The background helper writes its
# port after bind and is killed on both failure and success paths.
. ${srcdir:=.}/diag.sh init
require_plugin omelasticsearch
check_command_available python3

PORT_FILE="$RSYSLOG_DYNNAME.esfake.port"
COUNT_FILE="$RSYSLOG_DYNNAME.esfake.count"

test_error_exit_handler() {
	if [ -n "${SERVER_PID:-}" ]; then
		kill "$SERVER_PID" 2>/dev/null || true
	fi
}

python3 - "$PORT_FILE" "$COUNT_FILE" <<'PY' &
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

port_file, count_file = sys.argv[1:3]
post_count = 0


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
                "number": "8.15.0",
                "distribution": "elasticsearch",
            }
        })

    def do_POST(self):
        global post_count
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8")
        ops = max(1, len([line for line in body.splitlines() if line.strip()]) // 2)
        post_count += 1
        with open(count_file, "w", encoding="ascii") as fh:
            fh.write(f"{post_count}\n")
        if post_count >= 2:
            self.send_json({
                "errors": False,
                "items": [{"index": {"status": 201}} for _ in range(ops)]
            })
        else:
            self.send_json({
                "errors": True,
                "items": [{
                    "index": {
                        "status": 403,
                        "error": {
                            "type": "cluster_block_exception",
                            "reason": "blocked by: [FORBIDDEN/12/index read-only / allow delete (api)];",
                        },
                    }
                } for _ in range(ops)]
            })


server = HTTPServer(("127.0.0.1", 0), Handler)
with open(port_file, "w", encoding="ascii") as fh:
    fh.write(f"{server.server_port}\n")
server.serve_forever()
PY
SERVER_PID=$!

assign_file_content ES_PORT "$PORT_FILE"

generate_conf
add_conf '
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

template(name="msgTpl" type="string" string="{\"msg\":\"%msg%\"}")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
action(type="omelasticsearch"
       server="127.0.0.1"
       serverport="'$ES_PORT'"
       bulkmode="on"
       searchIndex="rsyslog_testbench"
       template="msgTpl"
       action.reportSuspension="on"
       action.resumeRetryCount="1"
       action.resumeInterval="1")
'

startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z host testprog - - - msgnum:0'

count=0
retry_timeout_seconds=$((TB_TIMEOUT_STARTSTOP / 10))
if [ "$retry_timeout_seconds" -lt 1 ]; then
	retry_timeout_seconds=1
fi
while [ "$count" -lt "$retry_timeout_seconds" ]; do
	if [ -r "$COUNT_FILE" ] && [ "$(cat "$COUNT_FILE")" -ge 2 ]; then
		break
	fi
	./msleep 1000
	count=$((count + 1))
done

if [ ! -r "$COUNT_FILE" ] || [ "$(cat "$COUNT_FILE")" -lt 2 ]; then
	echo "omelasticsearch did not retry the read-only bulk response"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

content_check "msgnum:0" "$RSYSLOG_OUT_LOG"

kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
exit_test
