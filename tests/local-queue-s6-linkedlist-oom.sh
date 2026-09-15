#!/bin/bash
# Hold actual FE IDs0/1 while the first LinkedList BE node allocation fails.
# ID2 is consumed as a preadmission rejection; the one-shot fault then permits
# ID3 to execute on BE. Exact 0,1,3 and conserved counters prove FE ownership,
# the failed reference disposition, and subsequent BE progress/capacity reuse.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
STATS="$PWD/$RSYSLOG_DYNNAME.stats"
ENTRY="$PWD/$RSYSLOG_DYNNAME.entry"
RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
STOPMARK="$PWD/$RSYSLOG_DYNNAME.stop"
export RSYSLOG_LOCAL_QUEUE_TEST_FAULT=backend-node
export RS_REDIR=">$PWD/$RSYSLOG_DYNNAME.fault.log 2>&1"
mkfifo "$RELEASE"
exec {release_fd}<>"$RELEASE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'$STATS'")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.scope="local" queue.type="LinkedList" queue.size="8"
 queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
 queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="0")
template(name="ids" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTRY' '$RELEASE';ids
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
'
startup
tcpflood -m1 -i0
wait_file_lines "$ENTRY" 1
tcpflood -m1 -i1
localq_wait_stats "$STATS" 'main Q.local' 'fe.queued.messages=1' 'fe.inflight.messages=1'
injectmsg 2 1
localq_wait_stats "$STATS" 'main Q.local' 'rejected.preadmission.messages=1' 'accepted.messages=2' 'be.physical.messages=0'
custom_content_check 'local queue test fault: backend-node' "$PWD/$RSYSLOG_DYNNAME.fault.log"
injectmsg 3 1
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 1
localq_wait_stats "$STATS" 'main Q.local' 'ingress.messages=4' 'accepted.messages=3' \
    'rejected.preadmission.messages=1' 'terminal.executed.messages=1' 'terminal.discarded.messages=0'
printf 'release\n' >&"$release_fd"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 'stop checker arm failed' ;; esac
shutdown_when_empty
wait_shutdown
wait_file_lines "$STOPMARK" 1
custom_content_check 'admitted=3 terminal=3 rejected=1' "$STOPMARK"
custom_content_check 'restored=0 persisted=0 executed=3 discarded=0' "$STOPMARK"
custom_content_check 'sampled_out=0 severity_discarded=0 retry_failed=0' "$STOPMARK"
python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import sys
from pathlib import Path
ids = [int(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert sorted(ids) == [0, 1, 3], ids
PY
[ "$?" -eq 0 ] || error_exit 1 'failed LinkedList admission changed ownership'
exec {release_fd}>&-
exit_test
