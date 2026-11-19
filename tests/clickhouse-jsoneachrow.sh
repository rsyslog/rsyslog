#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# JSONEachRow inserts put the query in the URL, post newline-delimited JSON,
# and can gzip that body with async_insert enabled. A bad table name, or a
# template with option.sql, option.stdsql, or option.json, is rejected at
# configuration time. At runtime the action uses an option.jsonf list
# template, and the stub answers HTTP 400 to every insert that contains message
# 1. The core retries a rejected batch one message at a time, so messages 0
# and 2 are inserted exactly once and message 1 is dropped. The batches the
# worker builds depend on timing, so the checks below do not count requests.
# The test waits until both good objects are accepted, then shuts down. A
# stall on the bad row would time out here. The error file is mode 0600 and
# holds only the uncompressed body of the rejected row.
# The stub writes its port file after bind. The EXIT trap kills it.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse
check_command_available python3

run_n1() {
    name="$1"
    needle="$2"
    expect_ok="$3"
    ../tools/rsyslogd -C -N1 -f "${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" \
        > "$RSYSLOG_DYNNAME.$name.log" 2>&1
    rc=$?
    if [ "$expect_ok" -eq 1 ]; then
        if [ "$rc" -ne 0 ]; then
            printf 'FAIL: %s config was rejected\n' "$name"
            cat "$RSYSLOG_DYNNAME.$name.log"
            error_exit 1
        fi
        content_check "End of config validation run" "$RSYSLOG_DYNNAME.$name.log"
    else
        if [ "$rc" -eq 0 ]; then
            printf 'FAIL: %s config was accepted\n' "$name"
            cat "$RSYSLOG_DYNNAME.$name.log"
            error_exit 1
        fi
        content_check "$needle" "$RSYSLOG_DYNNAME.$name.log"
    fi
}

json_module='module(load="../plugins/omclickhouse/.libs/omclickhouse")
template(name="chjson" type="string"
         string="{\"id\":\"%msg:F,58:2%\",\"message\":\"%msg%\"}")
template(name="chsql" type="string" option.stdsql="on"
         string="{\"id\":\"%msg:F,58:2%\",\"message\":\"%msg%\"}")
template(name="chjsonesc" type="string" option.json="on"
         string="{\"id\":\"%msg:F,58:2%\",\"message\":\"%msg%\"}")
template(name="chjsonf" type="list" option.jsonf="on") {
    property(outname="id" name="msg" field.delimiter="58" field.number="2" format="jsonf")
    property(outname="message" name="msg" format="jsonf")
}
'

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="jsoneachrow" table="bad name" template="chjson")
'
run_n1 bad-table "table must be" 0

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="jsoneachrow" table="a..b" template="chjson")
'
run_n1 two-dots "table must be" 0

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="jsoneachrow" table="db.1events" template="chjson")
'
run_n1 digit-start "table must be" 0

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="jsoneachrow" template="chjson")
'
run_n1 missing-table "table must be" 0

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="values" table="rsyslog.events" template="chjson")
'
run_n1 table-with-values "only valid with format jsoneachrow" 0

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="jsoneachrow" table="rsyslog.events" template="chsql")
'
run_n1 stdsql "option.stdsql" 0

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="jsoneachrow" table="rsyslog.events" template="chjsonesc")
'
run_n1 json "option.json" 0

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="jsoneachrow" table="rsyslog.events" template="chjsonf")
'
run_n1 jsonf "" 1

generate_conf
add_conf "$json_module"
add_conf '
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       format="nope" template="chjson")
'
run_n1 bad-format "format must be values or jsoneachrow" 0

export NUMMESSAGES=3
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
    --reject-match "msgnum:00000001:" &
stub_pid=$!
wait_file_exists "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf "$json_module"
add_conf '
:syslogtag, contains, "tag" action(type="omclickhouse"
    server="127.0.0.1"
    port="'"$port"'"
    usehttps="off"
    format="jsoneachrow"
    table="rsyslog.events"
    template="chjsonf"
    asyncinsert="on"
    compression="gzip"
    errorfile="'"$errfile"'"
    action.resumeInterval="1"
    action.resumeRetryCount="-1")
action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")
'

count_accepted_objects() {
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
                n += len([part for part in rec.get("body", "").split("\n") if part])
print(n)
PY
}

startup
injectmsg
wait_file_lines --count-function count_accepted_objects "$record" 2 30
shutdown_when_empty
wait_shutdown

content_check "omclickhouse: ClickHouse rejected the data with HTTP status 400: BAD REQUEST"

python3 - "$record" "$errfile" <<'PY' || error_exit 1
import json
import os
import sys

record, errfile = sys.argv[1], sys.argv[2]
recs = [json.loads(line) for line in open(record, encoding="utf-8") if line.strip()]
good = [
    {"id": "00000000", "message": " msgnum:00000000:"},
    {"id": "00000002", "message": " msgnum:00000002:"},
]
bad = {"id": "00000001", "message": " msgnum:00000001:"}
inserts = [rec for rec in recs if rec["kind"] == "insert"]
failed = [rec for rec in inserts if rec["status"] == 400]
ok = [rec for rec in inserts if rec["status"] == 200]
other = [rec for rec in inserts if rec["status"] not in (200, 400)]
if other:
    sys.stderr.write("unexpected insert records: {0}\n".format(json.dumps(inserts)))
    sys.exit(1)


def lines_of(body):
    return [json.loads(line) for line in body.split("\n") if line]


def check_insert(rec):
    path = rec["path"]
    for token in (
        "query=INSERT%20INTO%20rsyslog.events%20FORMAT%20JSONEachRow",
        "async_insert=1",
        "wait_for_async_insert=1",
        "enable_http_compression=1",
    ):
        if token not in path:
            sys.stderr.write("URL missing {0}: {1}\n".format(token, path))
            sys.exit(1)
    if "application/x-ndjson" not in rec["content_type"]:
        sys.stderr.write("content type: {0}\n".format(rec["content_type"]))
        sys.exit(1)
    if rec["content_encoding"].lower() != "gzip":
        sys.stderr.write("content encoding: {0}\n".format(rec["content_encoding"]))
        sys.exit(1)


for rec in inserts:
    check_insert(rec)
delivered = []
for rec in ok:
    delivered.extend(lines_of(rec["body"]))
if delivered != good:
    sys.stderr.write("accepted {0}\n".format(json.dumps(delivered)))
    sys.exit(1)
if not any(lines_of(rec["body"]) == [bad] for rec in failed):
    sys.stderr.write("bad row was never rejected alone: {0}\n".format(json.dumps(failed)))
    sys.exit(1)
mode = os.stat(errfile).st_mode & 0o777
if mode != 0o600:
    sys.stderr.write("error file mode {0:o}\n".format(mode))
    sys.exit(1)
errs = [json.loads(line) for line in open(errfile, encoding="utf-8") if line.strip()]
alone = [e for e in errs if lines_of(e["request"]["postdata"]) == [bad]]
if not alone or len(alone) != len(errs) or alone[0]["http_status"] != 400 or alone[0]["reply"] != "BAD REQUEST":
    sys.stderr.write("error file:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
if "rsyslog.events" not in alone[0]["request"]["url"]:
    sys.stderr.write("error file lost the target URL:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
PY

exit_test
