#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# A row that ClickHouse rejects with HTTP 400 must not block the rows around
# it or the messages after it. The stub answers 400 to every insert that
# contains message 1. Message 1 is written to the error file and dropped,
# every other message is inserted exactly once, and the error file holds
# nothing but message 1. A second round of messages is injected after the
# rejection to show that the worker still delivers.
# CH_OUTAGE=N makes the stub answer the first N inserts that hold message 1
# with HTTP 503, so the batch holding the bad row is suspended first and
# rejected only when it is retried; the check below asserts that order.
# CH_ALLOW_DUPLICATES=1 accepts a good row inserted more than once, which a
# retried transaction allows (delivery is at least once).
# CH_BULKMODE (default on) and CH_MAXBYTES (default unset) select the path:
# a bulk batch is retried one message at a time by the action engine, a
# non-bulk request or a maxBytes flush is final in the module.
# CH_REJECT_STATUS, CH_REJECT_CODE and CH_REJECT_BODY select how the stub
# rejects message 1. The default is what ClickHouse 26.8 sends for a row it
# cannot parse: HTTP 400, X-ClickHouse-Exception-Code 27, and a "Code: 27."
# body. CH_REJECT_CODE=none sends no code header.
# action.resumeRetryCount="-1" makes a stall run until a wait below times
# out. The batches the worker builds depend on timing, so the checks do not
# count requests. The stub writes its port file after bind. The EXIT trap
# kills it.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse
check_command_available python3

bulkmode="${CH_BULKMODE:-on}"
reject_status="${CH_REJECT_STATUS:-400}"
reject_code="${CH_REJECT_CODE:-27}"
reject_body="${CH_REJECT_BODY:-Code: 27. DB::Exception: Cannot parse input. (CANNOT_PARSE_INPUT_ASSERTION_FAILED)}"
code_arg=
if [ "$reject_code" != none ]; then
    code_arg="--reject-code=$reject_code"
fi
maxbytes_param=
if [ -n "${CH_MAXBYTES:-}" ]; then
    maxbytes_param="maxbytes=\"${CH_MAXBYTES}\""
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
    --reject-match "msgnum:00000001:" \
    --reject-status "$reject_status" \
    --reject-body "$reject_body" \
    --outage-match "msgnum:00000001:" \
    --outage-count "${CH_OUTAGE:-0}" \
    $code_arg &
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
                                   bulkmode="'"$bulkmode"'"
                                   '"$maxbytes_param"'
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
            if rec.get("kind") == "insert" and rec.get("status") == 200 and not rec.get("rejected"):
                n += len(re.findall(r"msgnum:\d+:", rec.get("body", "")))
print(n)
PY
}

if [ -n "${CH_ERRFILE_MODE:-}" ]; then
    : > "$errfile"
    chmod "$CH_ERRFILE_MODE" "$errfile"
fi
startup
injectmsg 0 3
wait_file_lines --count-function count_accepted_rows "$record" 2 30
injectmsg 3 3
wait_file_lines --count-function count_accepted_rows "$record" 5 30
shutdown_when_empty
wait_shutdown

content_check "omclickhouse: ClickHouse rejected the data with HTTP status ${reject_status}: ${reject_body}"

python3 - "$record" "$errfile" "$reject_status" "$reject_body" "${CH_ALLOW_DUPLICATES:-0}" \
    "${CH_OUTAGE:-0}" <<'PY' || error_exit 1
import json
import os
import re
import sys

record, errfile = sys.argv[1], sys.argv[2]
reject_status, reject_body = int(sys.argv[3]), sys.argv[4]
allow_duplicates = sys.argv[5] == "1"
outage = int(sys.argv[6])


def rows(body):
    return re.findall(r"msgnum:(\d+):", body)


recs = [json.loads(line) for line in open(record, encoding="utf-8") if line.strip()]
inserts = [rec for rec in recs if rec["kind"] == "insert"]
ok = [rec for rec in inserts if rec["status"] == 200 and not rec["rejected"]]
failed = [rec for rec in inserts if rec["rejected"] and rec["status"] == reject_status]
if any(rec["status"] not in (200, reject_status, 503) for rec in inserts):
    sys.stderr.write("unexpected insert records: {0}\n".format(json.dumps(inserts)))
    sys.exit(1)
delivered = []
for rec in ok:
    delivered.extend(rows(rec["body"]))
good = ["00000000", "00000002", "00000003", "00000004", "00000005"]
if (sorted(set(delivered)) if allow_duplicates else sorted(delivered)) != good:
    sys.stderr.write("accepted rows {0}\n".format(delivered))
    sys.exit(1)
if outage > 0:
    order = [rec["status"] for rec in inserts if "00000001" in rows(rec["body"])]
    if 503 not in order or reject_status not in order or order.index(503) > order.index(reject_status):
        sys.stderr.write("the bad row was not suspended before it was rejected: {0}\n".format(order))
        sys.exit(1)
if not any(rows(rec["body"]) == ["00000001"] for rec in failed):
    sys.stderr.write("bad row was never rejected alone: {0}\n".format(json.dumps(failed)))
    sys.exit(1)
mode = os.stat(errfile).st_mode & 0o777
if mode != 0o600:
    sys.stderr.write("error file mode {0:o}\n".format(mode))
    sys.exit(1)
errs = [json.loads(line) for line in open(errfile, encoding="utf-8") if line.strip()]
if not errs or any(rows(e["request"]["postdata"]) != ["00000001"] for e in errs):
    sys.stderr.write("error file must hold only message 1:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
if errs[0]["http_status"] != reject_status or errs[0]["reply"] != reject_body:
    sys.stderr.write("error file:\n{0}\n".format(json.dumps(errs)))
    sys.exit(1)
PY

exit_test
