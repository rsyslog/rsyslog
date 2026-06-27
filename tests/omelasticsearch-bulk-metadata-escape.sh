#!/bin/bash
# Regression test for omelasticsearch bulk metadata JSON escaping. Dynamic
# metadata templates can be derived from remote message content; the oracle is
# the captured _bulk request body from a local HTTP endpoint. The first bulk
# metadata line must remain valid JSON and preserve attacker-shaped quotes as
# data inside _index, _type, _parent, and _id values. The local capture
# server writes its dynamic port before rsyslog starts and is killed by trap.
. ${srcdir:=.}/diag.sh init
require_plugin omelasticsearch

if ! command -v python3 >/dev/null 2>&1; then
	printf 'SKIP: python3 is required for local omelasticsearch capture server\n'
	skip_test
fi

cat > "$RSYSLOG_DYNNAME.capture-es.py" <<'PY'
import http.server
import json
import pathlib
import socketserver
import sys

base = pathlib.Path(sys.argv[1])

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def do_GET(self):
        data = json.dumps({
            'version': {
                'number': '8.0.0',
                'distribution': 'elasticsearch',
                'build_flavor': 'default',
            },
            'tagline': 'You Know, for Search',
        }).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        length = int(self.headers.get('Content-Length', '0'))
        body = self.rfile.read(length)
        (base.with_suffix('.bulk.path')).write_text(self.path)
        (base.with_suffix('.bulk.body')).write_bytes(body)
        data = b'{"errors":false,"items":[{"index":{"status":201}}]}'
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):
        return

with socketserver.TCPServer(('127.0.0.1', 0), Handler) as httpd:
    base.with_suffix('.capture.port').write_text(str(httpd.server_address[1]))
    httpd.serve_forever()
PY

python3 "$RSYSLOG_DYNNAME.capture-es.py" "$RSYSLOG_DYNNAME" &
capture_pid=$!
cleanup_capture() {
	kill "$capture_pid" 2>/dev/null || true
	wait "$capture_pid" 2>/dev/null || true
}
trap cleanup_capture EXIT

wait_file_exists "$RSYSLOG_DYNNAME.capture.port" 10
capture_port=$(cat "$RSYSLOG_DYNNAME.capture.port")

generate_conf
add_conf '
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
template(name="msgTpl" type="string" string="{\"msg\":\"%msg:::json%\"}")
template(name="idxTpl" type="string" string="rsyslog-%msg%")
template(name="typeTpl" type="string" string="type-%msg%")
template(name="parentTpl" type="string" string="parent-%msg%")
template(name="idTpl" type="string" string="id-%msg%")

if $msg contains "bad" then {
action(type="omelasticsearch"
       server="127.0.0.1"
       serverport="'$capture_port'"
       bulkmode="on"
       dynSearchIndex="on"
       searchIndex="idxTpl"
       dynSearchType="on"
       searchType="typeTpl"
       dynParent="on"
       parent="parentTpl"
       dynbulkid="on"
       bulkid="idTpl"
       template="msgTpl"
       esversion.major="7"
       action.resumeRetryCount="0"
       action.resumeInterval="1")
}
'

startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z host testprog - - - bad"quote\slash'
shutdown_when_empty
wait_shutdown
wait_file_exists "$RSYSLOG_DYNNAME.bulk.body" 10

python3 - "$RSYSLOG_DYNNAME.bulk.path" "$RSYSLOG_DYNNAME.bulk.body" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1]).read_text()
body = Path(sys.argv[2]).read_text()
lines = body.splitlines()
if path != '/_bulk':
    raise SystemExit('unexpected bulk path: ' + path)
if len(lines) != 2:
    raise SystemExit('unexpected bulk body line count: ' + repr(lines))
metadata = json.loads(lines[0])
document = json.loads(lines[1])
index = metadata['index']
expected = r'bad"quote\slash'
expected_values = {
    '_index': 'rsyslog-' + expected,
    '_type': 'type-' + expected,
    '_parent': 'parent-' + expected,
    '_id': 'id-' + expected,
}
if index != expected_values:
    raise SystemExit('unexpected escaped metadata: ' + repr(index))
if document != {'msg': expected}:
    raise SystemExit('unexpected escaped document: ' + repr(document))
PY
if [ $? -ne 0 ]; then
	error_exit 1
fi

exit_test