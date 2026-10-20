#!/bin/bash
# Actual omelasticsearch consumes a four-message BE batch on a real FE helper
# while the dedicated consumer is held. A local HTTP fixture returns one 429,
# one permanent 400, and two successes. The explicit downstream retry ruleset
# retries only 429 and records all routed outcomes. Exact HTTP ID inventory
# proves no successful/permanent item replay; final local conservation proves
# source completion. The maxbytes wrapper forces legacy PREVIOUS_COMMITTED
# partial flushes. Requests, output files, and helper counters are the oracles;
# timeouts only detect hangs. No Elasticsearch node download is required.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin omelasticsearch
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
check_command_available python3
PORT_FILE="$PWD/$RSYSLOG_DYNNAME.es.port"
TRACE="$PWD/$RSYSLOG_DYNNAME.es.trace"
SUCCESS="$PWD/$RSYSLOG_DYNNAME.es.success"
ROUTED="$PWD/$RSYSLOG_DYNNAME.es.routed"
STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
STOPMARK="$PWD/$RSYSLOG_DYNNAME.stop"
BE_ENTRY="$PWD/$RSYSLOG_DYNNAME.be-entry"
BE_RELEASE="$PWD/$RSYSLOG_DYNNAME.be-release"
mkfifo "$BE_RELEASE"
maxbytes=${LOCAL_QUEUE_ES_MAXBYTES:-100000}
test_error_exit_handler() {
    if [ -n "${SERVER_PID:-}" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
python3 - "$PORT_FILE" "$TRACE" "$SUCCESS" <<'PY' &
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

port_file, trace, success = sys.argv[1:]
seen = {}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, value):
        data = json.dumps(value).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        self.reply({"version": {"number": "8.15.0", "distribution": "elasticsearch"}})

    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        docs = [json.loads(line) for line in body.splitlines()]
        ids = [int(doc["id"]) for doc in docs[1::2]]
        items = []
        for ident in ids:
            seen[ident] = seen.get(ident, 0) + 1
            status = 400 if ident == 2 else 429 if ident == 1 and seen[ident] == 1 else 201
            result = {"status": status, "_index": "localq", "_id": str(ident)}
            if status != 201:
                result["error"] = {"type": "es_rejected_execution_exception" if status == 429
                                   else "mapper_parsing_exception", "reason": "intentional item failure"}
            else:
                with open(success, "a", encoding="ascii") as out:
                    out.write(f"{ident}\n")
            items.append({"index": result})
        with open(trace, "a", encoding="ascii") as out:
            out.write(json.dumps(ids) + "\n")
        self.reply({"errors": any(item["index"]["status"] != 201 for item in items), "items": items})


server = HTTPServer(("127.0.0.1", 0), Handler)
with open(port_file, "w", encoding="ascii") as out:
    out.write(f"{server.server_port}\n")
server.serve_forever()
PY
SERVER_PID=$!
assign_file_content ES_PORT "$PORT_FILE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
# The legacy barrier must be claimed before the ES no-legacy selector handler.
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="128" queue.workerThreads="1"
 queue.dequeueBatchSize="4" queue.local.frontendSize="8" queue.local.maxFrontends="1")
template(name="esdoc" type="string" string="{\"id\":\"%msg:F,58:2%\"}")
template(name="retrydoc" type="string" string="{\"id\":\"%$!id%\"}")
template(name="outcome" type="string" string="%$!id%:%$.omes!status%\n")
# This queue is a distinct acyclic target for ES reinjection. It must remain
# available while its upstream output callbacks still own retry obligations.
ruleset(name="es_retry" queue.scope="local" queue.type="FixedArray" queue.size="32"
 queue.dequeueBatchSize="4" queue.local.frontendSize="8" queue.local.maxFrontends="4") {
 action(type="omfile" file="'$ROUTED'" template="outcome")
 if ($.omes!status == 429) then action(type="omelasticsearch" server="127.0.0.1" serverport="'$ES_PORT'"
  bulkmode="on" template="retrydoc" searchIndex="localq" retryfailures="off" indextimeout="5000")
}
if ($msg contains "msgnum:00000099:") then :omtesting:file_barrier '$BE_ENTRY' '$BE_RELEASE';esdoc
if ($msg contains "msgnum:") and not ($msg contains "msgnum:00000099:") and not ($msg contains "msgnum:00000098:") then
 action(type="omelasticsearch" server="127.0.0.1" serverport="'$ES_PORT'" bulkmode="on" template="esdoc"
  searchIndex="localq" retryfailures="on" retryruleset="es_retry" maxbytes="'$maxbytes'" indextimeout="5000")
'
startup
injectmsg 99 1
wait_file_lines "$BE_ENTRY" 1
tcpflood -m1 -i98
localq_wait_stats_regex "$STATSFILE" 'main Q.local' 'help.waits=[1-9][0-9]*'
injectmsg 0 4
wait_file_lines --abort-on-oversize "$SUCCESS" 3
routed_count=4
[ "$maxbytes" -eq 1 ] && routed_count=2
wait_file_lines --abort-on-oversize "$ROUTED" "$routed_count"
localq_wait_stats "$STATSFILE" 'main Q.local' 'batch.help.messages.max=4' 'inflight.help=0'
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 ;; esac
localq_release_barrier "$BE_RELEASE"
shutdown_when_empty
wait_shutdown
wait_file_lines "$STOPMARK" 1
python3 - "$TRACE" "$SUCCESS" "$ROUTED" "$maxbytes" <<'PY'
import collections
import json
import sys

trace, success, routed, maxbytes = sys.argv[1:]
with open(trace, encoding="ascii") as inp:
    requests = [json.loads(line) for line in inp]
assert collections.Counter(ident for req in requests for ident in req) == {0: 1, 1: 2, 2: 1, 3: 1}, requests
assert max(map(len, requests)) == (1 if maxbytes == "1" else 4), requests
with open(success, encoding="ascii") as inp:
    assert sorted(map(int, inp)) == [0, 1, 3]
with open(routed, encoding="ascii") as inp:
    actual = sorted((int(ident), int(status)) for ident, status in (line.strip().split(":") for line in inp))
expected = [(1, 429), (2, 400)] if maxbytes == "1" else [(0, 201), (1, 429), (2, 400), (3, 201)]
assert actual == expected, actual
PY
[ "$?" -eq 0 ] || error_exit 1 'ES item/HTTP inventory mismatch'
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=''
exit_test
