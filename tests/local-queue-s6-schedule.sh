#!/bin/bash
# A closed dequeue window must hold a real imtcp FE's accepted ID without a
# callback. The main queue remains unscheduled for harness control traffic.
# Observe the scheduled worker's wait plus queued ownership before shutdown;
# existing DA save must interrupt that wait and preserve ID0 for a no-ingress
# restart with the window removed. Exact final ID and proper termination are
# the oracle; no elapsed-time success threshold or hours-long test sleep.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
export NUMMESSAGES=1
SPOOL="$PWD/$RSYSLOG_DYNNAME.spool"
STATS="$PWD/$RSYSLOG_DYNNAME.stats"
export RSYSLOG_DEBUG='debug nologfuncflow noprintmutexaction nostdout'
export RSYSLOG_DEBUGLOG="$PWD/$RSYSLOG_DYNNAME.debug"
# Keep the window several hours away even if setup crosses an hour boundary.
hour=$((10#$(date +%H)))
window_start=$(((hour + 6) % 24))
window_end=$(((window_start + 1) % 24))
mkdir -p "$SPOOL"
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(workDirectory="'$SPOOL'" processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'$STATS'")
template(name="ids" type="string" string="%msg:F,58:2%\n")
ruleset(name="scheduled" queue.scope="local" queue.type="FixedArray" queue.size="32"
 queue.workerThreads="1" queue.dequeueBatchSize="1"
 queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="0"
 queue.filename="scheduled" queue.diskQueueType="disk" queue.saveOnShutdown="on"
 queue.timeoutShutdown="100" queue.timeoutActionCompletion="100"
 queue.dequeueTimeBegin="'$window_start'" queue.dequeueTimeEnd="'$window_end'"
) {
 action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
}
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1" ruleset="scheduled"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
'
startup
tcpflood -m1 -i0
localq_wait_stats "$STATS" 'scheduled.local' 'accepted.messages=1' 'route.fe.messages=1' \
    'fe.queued.messages=1' 'fe.inflight.messages=0' 'be.physical.messages=0'
wait_content 'local-fe-1.*outside dequeue time window' "$RSYSLOG_DEBUGLOG"
[ ! -s "$RSYSLOG_OUT_LOG" ] || error_exit 1 'closed FE schedule executed a callback'
shutdown_immediate
wait_shutdown
# The shutdown wake may permit delivery before save. Otherwise the same
# accepted obligation must be in the configured persistent store.
if [ ! -s "$RSYSLOG_OUT_LOG" ]; then
    [ -s "$SPOOL/scheduled.qi" ] || error_exit 1 'scheduled FE obligation was neither delivered nor saved'
fi
sed -i '/queue.dequeueTimeBegin=/d' "$CONF_FILE"
startup
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown
seq_check
exit_test
