#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# An early maxBytes flush that is resent one row at a time and suspended
# partway must not repeat the rows it already handled. Five VALUES rows are
# injected; maxBytes=150 flushes rows 0 to 2 when row 3 arrives. The stub
# rejects every insert that holds message 1 with HTTP 400 and code 27, and
# answers the first insert that holds message 2 (and not message 1) with 503.
# So row 0 is inserted, row 1 is recorded and dropped, and row 2 suspends the
# flush. The held batch must keep only row 2 on, so that endTransaction()
# sends rows 2 to 4 once. The oracle: rows 0, 2, 3, 4 inserted exactly once,
# and exactly one error record, for message 1.
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
    --reject-match "msgnum:00000001:" \
    --reject-code 27 \
    --reject-body "Code: 27. DB::Exception: Cannot parse input. (CANNOT_PARSE_INPUT_ASSERTION_FAILED)" \
    --fail-match "msgnum:00000002:" &
stub_pid=$!
wait_file_exists "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.sql="on" type="string"
         string="INSERT INTO rsyslog.poison (id, message) VALUES (%msg:F,58:2%, '\''%msg%'\'')")

:syslogtag, contains, "tag" action(type="omclickhouse"
                                   server="127.0.0.1"
                                   port="'"$port"'"
                                   usehttps="off"
                                   bulkmode="on"
                                   maxbytes="150"
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
injectmsg 0 5
wait_file_lines --count-function count_accepted_rows "$record" 4 30
shutdown_when_empty
wait_shutdown

python3 - "$record" "$errfile" <<'PY' || error_exit 1
import json
import re
import sys

record, errfile = sys.argv[1], sys.argv[2]


def rows(body):
    return re.findall(r"msgnum:(\d+):", body)


recs = [json.loads(line) for line in open(record, encoding="utf-8") if line.strip()]
delivered = []
for rec in recs:
    if rec["kind"] == "insert" and not rec["rejected"]:
        delivered.extend(rows(rec["body"]))
if sorted(delivered) != ["00000000", "00000002", "00000003", "00000004"]:
    sys.stderr.write("accepted rows {0}\n".format(delivered))
    sys.exit(1)
errs = [json.loads(line) for line in open(errfile, encoding="utf-8") if line.strip()]
if [rows(e["request"]["postdata"]) for e in errs] != [["00000001"]]:
    sys.stderr.write("error file must hold message 1 once:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
PY

exit_test
