#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# format=values: a rendered row that is not an INSERT statement is a data
# rejection made by the module itself. The bad row is message 0, so without
# bulk mode it is the first row the worker sees, before any post set the URL.
# CH_BULKMODE (default on) selects bulk mode. It is written to the error file with
# http_status 0 and dropped, and the rows around it are inserted. The check
# ignores case, so a lowercase "insert into" row is sent normally. The stub
# rejects any body holding the bad row with HTTP 400 and code 62, as
# ClickHouse does for a syntax error, so the batch is resent one row at a time.
# The oracle: rows 1 to 4 inserted exactly once, and one error record for
# row 0 with http_status 0.
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
    --reject-match "garbage row" \
    --reject-code 62 \
    --reject-body "Code: 62. DB::Exception: Syntax error. (SYNTAX_ERROR)" &
stub_pid=$!
wait_file_exists "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" type="string" option.sql="on" string="%$!stmt%")

if $syslogtag contains "tag" then {
    set $.id = field($msg, 58, 2);
    if $.id == "00000000" then {
        set $!stmt = "garbage row";
    } else if $.id == "00000002" then {
        set $!stmt = "insert into rsyslog.t (id) values (" & $.id & ")";
    } else {
        set $!stmt = "INSERT INTO rsyslog.t (id) VALUES (" & $.id & ")";
    }
    action(type="omclickhouse"
           server="127.0.0.1"
           port="'"$port"'"
           usehttps="off"
           bulkmode="'"${CH_BULKMODE:-on}"'"
           template="outfmt"
           errorfile="'"$errfile"'"
           action.resumeInterval="1"
           action.resumeRetryCount="-1")
}

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
                n += len(re.findall(r"\((\d{8})\)", rec.get("body", "")))
print(n)
PY
}

startup
injectmsg 0 5
wait_file_lines --count-function count_accepted_rows "$record" 4 30
shutdown_when_empty
wait_shutdown

content_check "omclickhouse: dropping a row: message is not an INSERT query"

python3 - "$record" "$errfile" <<'PY' || error_exit 1
import json
import re
import sys

record, errfile = sys.argv[1], sys.argv[2]
recs = [json.loads(line) for line in open(record, encoding="utf-8") if line.strip()]
delivered = []
for rec in recs:
    if rec["kind"] == "insert" and not rec["rejected"]:
        delivered.extend(re.findall(r"\((\d{8})\)", rec["body"]))
if sorted(delivered) != ["00000001", "00000002", "00000003", "00000004"]:
    sys.stderr.write("accepted rows {0}\n".format(delivered))
    sys.exit(1)
if any("garbage row" in rec["body"] for rec in recs if not rec["rejected"]):
    sys.stderr.write("the bad row was accepted\n")
    sys.exit(1)
errs = [json.loads(line) for line in open(errfile, encoding="utf-8") if line.strip()]
if len(errs) != 1 or errs[0]["request"]["postdata"] != "garbage row" or errs[0]["http_status"] != 0 \
        or errs[0]["reply"] != "message is not an INSERT query":
    sys.stderr.write("error file:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
PY

exit_test
