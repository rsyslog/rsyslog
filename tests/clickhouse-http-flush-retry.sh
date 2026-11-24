#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# A temporary failure of an early maxBytes flush must not lose rows. With
# maxBytes=100 each row after the first is sent by a flush inside doAction().
# The stub answers the first insert with HTTP 503. The flushed rows stay in
# the batch and endTransaction() or the engine's retry sends them again, so
# every message is inserted exactly once and nothing reaches the error file.
# The oracle is the set of accepted rows; a lost row would leave it short and
# the wait below would time out.
# The stub writes its port file after bind. The EXIT trap kills it.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse
check_command_available python3

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
    --reject-status 503 &
stub_pid=$!
wait_file_exists "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.sql="on" type="string"
         string="INSERT INTO rsyslog.flush (id, message) VALUES (%msg:F,58:2%, '\''%msg%'\'')")

:syslogtag, contains, "tag" action(type="omclickhouse"
                                   server="127.0.0.1"
                                   port="'"$port"'"
                                   usehttps="off"
                                   bulkmode="on"
                                   maxbytes="100"
                                   template="outfmt"
                                   errorfile="'"$errfile"'"
                                   action.resumeInterval="1"
                                   action.resumeRetryCount="-1")

action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")
'

count_accepted_rows() {
    python3 - "$record" <<'PY'
import json
import os
import re
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
            if rec.get("kind") == "insert" and not rec.get("rejected"):
                n += len(re.findall(r"msgnum:\d+:", rec.get("body", "")))
print(n)
PY
}

startup
injectmsg 0 3
wait_file_lines --count-function count_accepted_rows "$record" 3 30
shutdown_when_empty
wait_shutdown

content_check "omclickhouse: ClickHouse request failed with HTTP status 503"
if [ -s "$errfile" ]; then
    printf 'FAIL: a retried 503 was written to the error file\n'
    cat "$errfile"
    error_exit 1
fi

python3 - "$record" <<'PY' || error_exit 1
import json
import re
import sys

recs = [json.loads(line) for line in open(sys.argv[1], encoding="utf-8") if line.strip()]
delivered = []
for rec in recs:
    if rec["kind"] == "insert" and not rec["rejected"]:
        delivered.extend(re.findall(r"msgnum:(\d+):", rec["body"]))
if sorted(delivered) != ["00000000", "00000001", "00000002"]:
    sys.stderr.write("accepted rows {0}\n".format(delivered))
    sys.exit(1)
PY

exit_test
