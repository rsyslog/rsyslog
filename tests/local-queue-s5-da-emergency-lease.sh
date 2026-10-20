#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A local DA transfer acquires one parent message while an output callback
# holds ID0. The test-only fault detaches an empty disk child immediately after
# that dequeue, exercising ConsumerDA's post-dequeue NULL destination branch.
# Its debug marker proves the branch retired its lease. Exact output IDs and
# the final local-queue stop check prove all accepted references reached one
# terminal outcome, without loss or duplicate delivery. Waiting is only for
# these observable events; the watchdog bounds a stuck worker.
. ${srcdir:=.}/diag.sh init
check_command_available python3
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin omtesting
export NUMMESSAGES=31
export RSYSLOG_LOCAL_QUEUE_TEST_DA_DETACH_AFTER_DEQUEUE=1
export RSYSLOG_DEBUG='debug nologfuncflow noprintmutexaction nostdout'
export RSYSLOG_DEBUGLOG="$PWD/$RSYSLOG_DYNNAME.debug"
SPOOL="$PWD/$RSYSLOG_DYNNAME.spool"
BE_ENTRY="$PWD/$RSYSLOG_DYNNAME.be-entry"
BE_RELEASE="$PWD/$RSYSLOG_DYNNAME.be-release"
STOPMARK="$PWD/$RSYSLOG_DYNNAME.stop"
mkdir -p "$SPOOL"
mkfifo "$BE_RELEASE"
exec {be_fd}<>"$BE_RELEASE"
# error_exit calls this before its daemon shutdown, so a missing DA marker
# cannot leave the held callback waiting for timeoutGuard.
test_error_exit_handler() {
    printf 'release\n' >&"$be_fd" || true
    exec {be_fd}>&-
}
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(workDirectory="'$SPOOL'" processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.scope="local" queue.type="FixedArray" queue.filename="daemergency"
 queue.size="64" queue.highWatermark="8" queue.lowWatermark="4"
 queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
 queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="0")
template(name="ids" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$BE_ENTRY' '$BE_RELEASE';ids
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
'
startup
tcpflood -m1 -i0
wait_file_lines "$BE_ENTRY" 1
tcpflood -m30 -i1
wait_content 'ConsumerDA: detached disk child; parent lease retired' "$RSYSLOG_DEBUGLOG"
printf 'release\n' >&"$be_fd"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
wait_queueempty
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 'stop checker arm failed' ;; esac
shutdown_when_empty
wait_shutdown
wait_file_lines "$STOPMARK" 1
python3 - "$RSYSLOG_OUT_LOG" "$STOPMARK" <<'PY'
import re
import sys
from pathlib import Path

output, marker = sys.argv[1:]
ids = [int(line) for line in Path(output).read_text().splitlines()]
fields = dict(re.findall(r'(?:^|\s)([a-z_.]+)=(\d+)', Path(marker).read_text()))
assert len(ids) == 31 and set(ids) == set(range(31)), ids
assert int(fields['admitted']) == 31, fields
assert int(fields['executed']) == 31, fields
assert int(fields['discarded']) == 0, fields
assert int(fields['persisted']) == int(fields['restored']) == 0, fields
PY
[ "$?" -eq 0 ] || error_exit 1 'detached DA parent lease did not reconcile'
exec {be_fd}>&-
unset -f test_error_exit_handler
exit_test
