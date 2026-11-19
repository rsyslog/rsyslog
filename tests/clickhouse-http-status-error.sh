#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression coverage for GitHub issues #3485 and #3487, plus the suspend
# contract. An HTTP 503 whose body is not a DB::Exception string is still a
# ClickHouse failure, the failure is logged, and the message is not
# acknowledged. It is not a data error, so nothing is written to the error
# file. The stub answers the first insert with 503 and then accepts it.
# CH_STATUS, CH_CODE and CH_BODY change that first answer; CH_CODE=none sends
# no X-ClickHouse-Exception-Code header.
# action.resumeInterval="1" is the configured retry delay. The message has
# already left the main queue while that retry is in progress, so
# shutdown_when_empty would stop rsyslogd before the second POST. The test
# waits until the stub records one accepted insert, then shuts down. A
# module that acknowledged the 503 never writes that HTTP 200 record.
# The stub writes its port file after bind. The EXIT trap kills it.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse
check_command_available python3

export NUMMESSAGES=1
status="${CH_STATUS:-503}"
body="${CH_BODY:-BAD REQUEST}"
code_arg=
if [ "${CH_CODE:-none}" != none ]; then
    code_arg="--reject-code=$CH_CODE"
fi
port_file="${RSYSLOG_DYNNAME}.ch.port"
record="${RSYSLOG_DYNNAME}.ch.jsonl"
errfile="${RSYSLOG_DYNNAME}.ch.err"
stub_pid=

cleanup_stub() {
    if [ -n "${stub_pid}" ] && kill -0 "$stub_pid" 2>/dev/null; then
        kill "$stub_pid" 2>/dev/null || true
        wait "$stub_pid" 2>/dev/null || true
    fi
}
trap cleanup_stub EXIT

python3 "${srcdir}/clickhouse_http_stub.py" \
    --port-file "$port_file" \
    --record "$record" \
    --reject-inserts 1 \
    --reject-status "$status" \
    --reject-body "$body" \
    $code_arg &
stub_pid=$!
wait_file_exists "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.sql="on" type="string"
         string="INSERT INTO rsyslog.statuserror (id, severity, message) VALUES (%msg:F,58:2%, %syslogseverity%, '\''%msg%'\'')")

:syslogtag, contains, "tag" action(type="omclickhouse"
                                   server="127.0.0.1"
                                   port="'"$port"'"
                                   usehttps="off"
                                   bulkmode="off"
                                   template="outfmt"
                                   errorfile="'"$errfile"'"
                                   action.resumeInterval="1")

action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")
'

count_accepted_inserts() {
    python3 - "$record" <<'PY'
import json
import os
import sys

n = 0
path = sys.argv[1]
if os.path.exists(path):
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except ValueError:
                continue
            if rec.get("kind") == "insert" and rec.get("status") == 200:
                n += 1
print(n)
PY
}

startup
injectmsg
wait_file_lines --count-function count_accepted_inserts "$record" 1 30
shutdown_when_empty
wait_shutdown

content_check "omclickhouse: ClickHouse request failed with HTTP status ${status}: ${body}"
if [ -s "$errfile" ]; then
    printf 'FAIL: a retried %s was written to the error file\n' "$status"
    cat "$errfile"
    error_exit 1
fi

python3 - "$record" <<'PY' || error_exit 1
import json
import sys

recs = [json.loads(line) for line in open(sys.argv[1], encoding="utf-8") if line.strip()]
inserts = [rec for rec in recs if rec["kind"] == "insert"]
failed = [rec for rec in inserts if rec["rejected"]]
ok = [rec for rec in inserts if rec["status"] == 200]
if len(failed) != 1 or len(ok) != 1:
    sys.stderr.write("unexpected insert records: {0}\n".format(inserts))
    sys.exit(1)
if ok[0]["path"] != "/":
    sys.stderr.write("values mode changed the URL: {0}\n".format(ok[0]["path"]))
    sys.exit(1)
if "application/json" not in ok[0]["content_type"]:
    sys.stderr.write("values mode content type: {0}\n".format(ok[0]["content_type"]))
    sys.exit(1)
if "INSERT INTO" not in ok[0]["body"] or "msgnum:00000000" not in ok[0]["body"]:
    sys.stderr.write("retried body missing the insert: {0}\n".format(ok[0]["body"]))
    sys.exit(1)
if ok[0]["content_encoding"]:
    sys.stderr.write("values mode set Content-Encoding: {0}\n".format(ok[0]["content_encoding"]))
    sys.exit(1)
PY

exit_test
