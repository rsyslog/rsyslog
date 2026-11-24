#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# maxConsecutiveRejects stops dropping when every row is rejected, as after a
# schema change. Each message is its own request (bulk mode off). The stub
# rejects the first six inserts with HTTP 400 and code 27, then accepts. With
# a limit of 2, messages 0 and 1 are dropped into the error file. Message 2 is
# rejected again but kept: the action suspends and retries it until the stub
# accepts it, then messages 3 and 4 follow. The engine retries a suspended
# transaction from its first message, so messages 0 and 1 may come back and
# be inserted too once rows are accepted. The oracle: messages 2 to 4 are
# inserted, every message is inserted or in the error file, and the error
# file holds only messages 0 and 1. A drop of message 2 would leave the
# accepted set short and the wait below would time out.
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
    --reject-inserts 6 \
    --reject-code 27 \
    --reject-body "Code: 27. DB::Exception: Cannot parse input. (CANNOT_PARSE_INPUT_ASSERTION_FAILED)" &
stub_pid=$!
wait_file_exists "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.sql="on" type="string"
         string="INSERT INTO rsyslog.limit (id, message) VALUES (%msg:F,58:2%, '\''%msg%'\'')")

:syslogtag, contains, "tag" action(type="omclickhouse"
                                   server="127.0.0.1"
                                   port="'"$port"'"
                                   usehttps="off"
                                   bulkmode="off"
                                   maxconsecutiverejects="2"
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
wait_file_lines --count-function count_accepted_rows "$record" 3 60
shutdown_when_empty
wait_shutdown

content_check "2 rows in a row were rejected"
content_check "ClickHouse accepts rows again"

python3 - "$record" "$errfile" <<'PY' || error_exit 1
import json
import re
import sys

record, errfile = sys.argv[1], sys.argv[2]


def rows(body):
    return re.findall(r"msgnum:(\d+):", body)


recs = [json.loads(line) for line in open(record, encoding="utf-8") if line.strip()]
inserts = [rec for rec in recs if rec["kind"] == "insert"]
delivered = set()
for rec in inserts:
    if not rec["rejected"]:
        delivered.update(rows(rec["body"]))
errs = [json.loads(line) for line in open(errfile, encoding="utf-8") if line.strip()]
dropped = [rows(e["request"]["postdata"]) for e in errs]
if not {"00000002", "00000003", "00000004"} <= delivered:
    sys.stderr.write("accepted rows {0}\n".format(sorted(delivered)))
    sys.exit(1)
if sorted(dropped) != [["00000000"], ["00000001"]]:
    sys.stderr.write("error file must hold messages 0 and 1:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
everything = delivered | {row for rec in dropped for row in rec}
if everything != {"0000000%d" % i for i in range(5)}:
    sys.stderr.write("lost messages: delivered {0}, dropped {1}\n".format(sorted(delivered), dropped))
    sys.exit(1)
PY

exit_test
