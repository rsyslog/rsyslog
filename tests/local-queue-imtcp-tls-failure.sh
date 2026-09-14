#!/bin/bash
# Force the selected local-registration allocation/start failure in an actual
# initialized daemon. One real imtcp worker submits exact IDs; all must reach BE
# with the registration-fallback reason and no FE publication or FE worker.
# The worker-start case consumes one permanently failed descriptor; allocation
# failures consume none. Snapshots and exact unique output are the oracle, with
# normal daemon termination proving failed-registration cleanup is safe.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats

export RSYSLOG_LOCAL_QUEUE_TEST_FAULT="${LOCALQ_TEST_FAULT:-producer-tls}"
export RS_REDIR=">${RSYSLOG_DYNNAME}.fault.log 2>&1"
export NUMMESSAGES=73
export RSYSLOG_OUT_LOG="$PWD/$RSYSLOG_OUT_LOG"
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
registered=0
[ "$RSYSLOG_LOCAL_QUEUE_TEST_FAULT" != frontend-worker ] || registered=1

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="128"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1"
    queue.local.frontendSize="8" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="failurefmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="failurefmt")
'
startup
content_check "local queue test fault: $RSYSLOG_LOCAL_QUEUE_TEST_FAULT" "${RSYSLOG_DYNNAME}.fault.log"
tcpflood -m "$NUMMESSAGES"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
localq_wait_stats_regex "$STATSFILE" 'main Q.local' \
    "fe.registered=$registered" 'fe.started=0' 'route.fe.messages=0' \
    "route.be.reason.registration_fallback.messages=$NUMMESSAGES" \
    'fe.registration_failures=[1-9][0-9]*' 'outstanding.messages=0'
if [ "$registered" -eq 1 ]; then
    localq_wait_stats "$STATSFILE" 'main Q.local.frontend.1' \
        'registration.id=1' 'registration.generation=1' 'lifecycle.state=3' \
        'admitted.messages=0' 'inflight.fe=0' 'queued=0'
fi
shutdown_when_empty
wait_shutdown
seq_check
exit_test
