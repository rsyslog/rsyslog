#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# A rejected row is dropped only once it is recorded. The error file is in a
# directory that does not exist yet, so recording message 1 fails: the action
# must suspend and keep the row queued. The test waits for that log line,
# creates the directory, and then expects message 1 in the error file once
# and messages 0, 2, 3 inserted. Each message is its own request (bulk mode
# off). The engine retries a suspended transaction from its first message, so
# message 0 may be inserted twice; only the set of inserted rows is checked.
# The action has its own queue, as in production: on the main queue its
# retries would hold back rsyslogd's own messages, which the wait reads.
# The stub writes its port file after bind. The EXIT trap kills it.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse
check_command_available python3

port_file="${RSYSLOG_DYNNAME}.ch.port"
record="${RSYSLOG_DYNNAME}.ch.jsonl"
errdir="${RSYSLOG_DYNNAME}.errdir"
errfile="${errdir}/ch.err"
stub_pid=

cleanup_stub() {
    if [ -n "${stub_pid}" ] && kill -0 "$stub_pid" 2>/dev/null; then
        kill "$stub_pid" 2>/dev/null || true
        wait "$stub_pid" 2>/dev/null || true
    fi
    rm -rf "$errdir"
}
trap cleanup_stub EXIT

python3 "${srcdir}/clickhouse_http_stub.py" \
    --port-file "$port_file" \
    --record "$record" \
    --reject-match "msgnum:00000001:" \
    --reject-code 27 \
    --reject-body "Code: 27. DB::Exception: Cannot parse input. (CANNOT_PARSE_INPUT_ASSERTION_FAILED)" &
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
                                   bulkmode="off"
                                   template="outfmt"
                                   errorfile="'"$errfile"'"
                                   queue.type="LinkedList"
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

seen = set()
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
                seen.update(re.findall(r"msgnum:(\d+):", rec.get("body", "")))
print(len(seen))
PY
}

startup
injectmsg 0 4
wait_content "could not record a rejected row in the error file, keeping it queued"
if [ -e "$errfile" ]; then
    printf 'FAIL: error file exists before its directory\n'
    error_exit 1
fi
mkdir "$errdir"
wait_file_lines --count-function count_accepted_rows "$record" 3 30
wait_file_exists "$errfile"
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
delivered = set()
for rec in recs:
    if rec["kind"] == "insert" and not rec["rejected"]:
        delivered.update(rows(rec["body"]))
if delivered != {"00000000", "00000002", "00000003"}:
    sys.stderr.write("accepted rows {0}\n".format(sorted(delivered)))
    sys.exit(1)
errs = [json.loads(line) for line in open(errfile, encoding="utf-8") if line.strip()]
if [rows(e["request"]["postdata"]) for e in errs] != [["00000001"]]:
    sys.stderr.write("error file must hold message 1 once:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
PY

exit_test
