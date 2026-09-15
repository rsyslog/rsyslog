#!/bin/bash
# Hold FE callback0 and queue ID1 behind it. With batch size1, configured 400ms
# slowdown must separate their callbacks. Start a monotonic clock immediately
# before releasing callback0; seeing both exact IDs cannot take less than 300ms.
# The 100ms allowance avoids an exact sleep-duration claim; no upper latency or
# performance assertion is made. FE route/inventory proves BE cannot satisfy it.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
export NUMMESSAGES=2
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
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32"
 queue.workerThreads="1" queue.dequeueBatchSize="1" queue.dequeueSlowdown="400000"
 queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="0")
template(name="ids" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTRY' '$RELEASE';ids
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
'
startup
tcpflood -m1 -i0
wait_file_lines "$ENTRY" 1
tcpflood -m1 -i1
localq_wait_stats "$STATS" 'main Q.local' 'route.fe.messages=2' 'fe.queued.messages=1' 'fe.inflight.messages=1'
# One observer releases the FIFO and checks output without coarse harness
# polling, which could otherwise manufacture the lower-bound timing result.
python3 - "$RELEASE" "$RSYSLOG_OUT_LOG" "$TB_TEST_TIMEOUT" <<'PY'
import sys
import time
from pathlib import Path
release, output, watchdog = sys.argv[1:]
started = time.monotonic()
with open(release, 'w') as stream:
    stream.write('release\n')
while time.monotonic() - started < int(watchdog):
    if Path(output).exists() and len(Path(output).read_text().splitlines()) >= 2:
        elapsed = time.monotonic() - started
        assert elapsed >= 0.3, elapsed
        break
    time.sleep(0.005)
else:
    raise AssertionError('second FE callback did not arrive')
PY
[ "$?" -eq 0 ] || error_exit 1 'FE slowdown was not applied between callbacks'
shutdown_when_empty
wait_shutdown
seq_check
exec {release_fd}>&-
exit_test
