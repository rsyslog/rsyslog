#!/bin/bash
# Severity pressure is logical: FE inflight ID0 plus queued ID1 reaches mark2
# while BE is empty. A Direct main queue parses priority before calling the
# local ruleset on the same imtcp producer; admission itself does not parse. Low-priority IDs2..4 must be policy drops, critical ID5
# must survive, and low-priority ID6 must be accepted after FE completion.
# Exact IDs distinguish policy drops from accepted terminal or admission errors;
# fixed resource metrics describe B+N*(F+D)=8+1*(4+3)=15, not current occupancy.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
STATS="$PWD/$RSYSLOG_DYNNAME.stats"
ENTRY="$PWD/$RSYSLOG_DYNNAME.entry"
RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
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
main_queue(queue.type="Direct")
template(name="ids" type="string" string="%msg:F,58:2%\n")
ruleset(name="policy" queue.scope="local" queue.type="'${LOCAL_QUEUE_TEST_BE_TYPE:-FixedArray}'" queue.size="8"
 queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="3"
 queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="0"
 queue.discardMark="2" queue.discardSeverity="5") {
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTRY' '$RELEASE';ids
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
}
if ($msg contains "msgnum:") then call policy
'
startup
tcpflood -m1 -i0 -P160
wait_file_lines "$ENTRY" 1
tcpflood -m1 -i1 -P160
localq_wait_stats "$STATS" 'policy.local' 'fe.inflight.messages=1' 'fe.queued.messages=1' 'be.physical.messages=0'
tcpflood -m3 -i2 -P167
localq_wait_stats "$STATS" 'policy.local' 'policy.severity_discarded.messages=3' 'accepted.messages=2' \
    'policy.sampled_out.messages=0' 'rejected.preadmission.messages=0'
tcpflood -m1 -i5 -P160
localq_wait_stats "$STATS" 'policy.local' 'fe.queued.messages=2' 'accepted.messages=3' \
    'resource.reserved.messages=15' 'resource.be.capacity.messages=8' \
    'resource.fe.capacity.messages=4' 'resource.fe.active.capacity.messages=3'
printf 'release\n' >&"$release_fd"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3
localq_wait_stats "$STATS" 'policy.local' 'outstanding.messages=0'
tcpflood -m1 -i6 -P167
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 4
localq_wait_stats "$STATS" 'policy.local' 'ingress.messages=7' 'accepted.messages=4' \
    'policy.severity_discarded.messages=3' 'rejected.preadmission.messages=0' 'outstanding.messages=0'
localq_wait_stats "$STATS" 'policy.local' 'terminal.executed.messages=4' \
    'terminal.discarded.messages=0' 'outstanding.messages=0'
shutdown_when_empty
wait_shutdown
python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import sys
from pathlib import Path
ids = [int(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert sorted(ids) == [0, 1, 5, 6], ids
PY
[ "$?" -eq 0 ] || error_exit 1 'logical severity policy changed destination inventory'
exec {release_fd}>&-
exit_test
