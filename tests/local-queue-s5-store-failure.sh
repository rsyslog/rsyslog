#!/bin/bash
# A real local FE holds IDs0/1 and BE holds ID2 while the spool directory is
# replaced by a regular file. A DA enqueue-error trace proves storage failed;
# only then restore it and release callbacks. Segmented DA retains every ID.
# Classic DA keeps its existing discard policy: unique delivered IDs plus the
# final explicit discard count must account for every accepted ID exactly once.
# IDs0/1/2 never visited the failed store and must reach their destination.
# The wait watchdog bounds a broken retry path; time alone proves nothing.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
export NUMMESSAGES=23
engine=${LOCAL_QUEUE_S5_ENGINE:-disk}
SPOOL="$PWD/$RSYSLOG_DYNNAME.spool"
STATS="$PWD/$RSYSLOG_DYNNAME.stats"
STOPMARK="$PWD/$RSYSLOG_DYNNAME.stop"
FE_ENTRY="$PWD/$RSYSLOG_DYNNAME.fe-entry"
BE_ENTRY="$PWD/$RSYSLOG_DYNNAME.be-entry"
FE_RELEASE="$PWD/$RSYSLOG_DYNNAME.fe-release"
BE_RELEASE="$PWD/$RSYSLOG_DYNNAME.be-release"
export RSYSLOG_DEBUG='debug nologfuncflow noprintmutexaction nostdout'
export RSYSLOG_DEBUGLOG="$PWD/$RSYSLOG_DYNNAME.debug"
mkdir -p "$SPOOL"
mkfifo "$FE_RELEASE" "$BE_RELEASE"
exec {fe_fd}<>"$FE_RELEASE"
exec {be_fd}<>"$BE_RELEASE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(workDirectory="'$SPOOL'" processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'$STATS'")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.scope="local" queue.type="FixedArray" queue.filename="s5failure" queue.size="32"
 queue.highWatermark="8" queue.lowWatermark="4" queue.workerThreads="1" queue.workerThreadMinimumMessages="1"
 queue.dequeueBatchSize="1" queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="0"
 queue.diskQueueType="'$engine'" queue.saveOnShutdown="on")
template(name="ids" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$FE_ENTRY' '$FE_RELEASE';ids
if ($msg contains "msgnum:00000002:") then :omtesting:file_barrier '$BE_ENTRY' '$BE_RELEASE';ids
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
'
startup
tcpflood -m1 -i0
wait_file_lines "$FE_ENTRY" 1
tcpflood -m1 -i1
localq_wait_stats "$STATS" 'main Q.local' 'fe.queued.messages=1' 'fe.inflight.messages=1'
injectmsg 2 1
wait_file_lines "$BE_ENTRY" 1
# Directory replacement fails path traversal even for privileged test runners.
# No disk message has been admitted yet, so this never hides an owned segment.
mv "$SPOOL" "$SPOOL.available"
printf 'unavailable\n' > "$SPOOL"
injectmsg 3 20
wait_content 'ConsumerDA:qqueueEnqMsg item (0) returned with error state:' "$RSYSLOG_DEBUGLOG"
rm "$SPOOL"
mv "$SPOOL.available" "$SPOOL"
printf 'release\n' >&"$fe_fd"
printf 'release\n' >&"$be_fd"
wait_queueempty
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 'stop checker arm failed' ;; esac
shutdown_when_empty
wait_shutdown
wait_file_lines "$STOPMARK" 1
python3 - "$RSYSLOG_OUT_LOG" "$STOPMARK" "$engine" <<'PY'
import re
import sys
from pathlib import Path
output, marker, engine = sys.argv[1:]
ids = [int(line) for line in Path(output).read_text().splitlines()]
fields = dict(re.findall(r'([a-z.]+)=(\d+)', Path(marker).read_text()))
assert len(ids) == len(set(ids)), ids
assert set(ids) <= set(range(23)) and {0, 1, 2} <= set(ids), ids
assert int(fields['admitted']) == 23, fields
assert int(fields['executed']) == len(ids), (fields, ids)
assert int(fields['persisted']) == int(fields['restored']) == 0, fields
assert len(ids) + int(fields['discarded']) == 23, (fields, ids)
if engine == 'segmentedDisk':
    assert set(ids) == set(range(23)) and int(fields['discarded']) == 0, (fields, ids)
PY
[ "$?" -eq 0 ] || error_exit 1 'storage failure ownership reconciliation failed'
exec {fe_fd}>&-
exec {be_fd}>&-
exit_test
