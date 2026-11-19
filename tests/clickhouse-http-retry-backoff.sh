#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# An insert that ClickHouse keeps failing while the server is up (here HTTP
# 404, code 60, a missing table) must not be retried in a tight loop. The
# SELECT 1 health check succeeds throughout, and with
# action.resumeRetryCount="0" the engine does not sleep before retrying a
# transaction, so only the module's own backoff in tryResume() stops it.
# The module retries one failed insert at once and reports a suspension after
# the second failure in a row; with this retry count the engine then suspends
# the action and discards the batch. The stub fails every insert. The oracle:
# at least two inserts (the immediate retry happened) and at most 20 in about
# four seconds (a tight loop posts thousands), plus the suspension in the log.
# The stub writes its port file after bind. The EXIT trap kills it.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse
check_command_available python3

port_file="${RSYSLOG_DYNNAME}.ch.port"
record="${RSYSLOG_DYNNAME}.ch.jsonl"
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
    --reject-inserts 1000000 \
    --reject-status 404 \
    --reject-code 60 \
    --reject-body "Code: 60. DB::Exception: Table rsyslog.backoff does not exist. (UNKNOWN_TABLE)" &
stub_pid=$!
wait_file_exists "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.sql="on" type="string"
         string="INSERT INTO rsyslog.backoff (id, message) VALUES (%msg:F,58:2%, '\''%msg%'\'')")

:syslogtag, contains, "tag" action(type="omclickhouse"
                                   server="127.0.0.1"
                                   port="'"$port"'"
                                   usehttps="off"
                                   bulkmode="on"
                                   template="outfmt"
                                   queue.type="LinkedList"
                                   action.resumeInterval="1"
                                   action.resumeRetryCount="0")

action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")
'

startup
injectmsg 0 1
wait_content "ClickHouse request failed with HTTP status 404"
$TESTTOOL_DIR/msleep 4000
shutdown_immediate
wait_shutdown
content_check "/.libs/omclickhouse'), next retry is"

python3 - "$record" <<'PY' || error_exit 1
import json
import sys

recs = [json.loads(line) for line in open(sys.argv[1], encoding="utf-8") if line.strip()]
inserts = [rec for rec in recs if rec["kind"] == "insert"]
if len(inserts) < 2:
    sys.stderr.write("{0} insert(s): the failed insert was not retried\n".format(len(inserts)))
    sys.exit(1)
if len(inserts) > 20:
    sys.stderr.write("{0} inserts in about four seconds: the action does not back off\n".format(len(inserts)))
    sys.exit(1)
PY

exit_test
