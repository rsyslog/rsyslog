#!/bin/bash
# The real TCP target is bound but never listens, so omfwd fails connection
# establishment in beginTransaction. Finite retries must exhaust and the
# worker-local previous-suspension state must select the Direct file fallback
# for every FE/BE-origin message. Exact fallback IDs are the delivery oracle;
# this does not claim that a failure discovered only at a later transaction
# commit can retroactively select a script fallback. No port-preselection race.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
export NUMMESSAGES=4
LISTEN_RELEASE="$PWD/$RSYSLOG_DYNNAME.never-listen"
start_minitcpsrvr "$RSYSLOG2_OUT_LOG" 1 "$LISTEN_RELEASE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32" queue.workerThreads="1"
 queue.dequeueBatchSize="2" queue.local.frontendSize="8" queue.local.maxFrontends="1")
template(name="wire" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
 action(type="omfwd" protocol="tcp" target="127.0.0.1" port="'$MINITCPSRVR_PORT1'"
  template="wire" action.resumeRetryCount="1" action.resumeInterval="1")
 action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="wire" action.execOnlyWhenPreviousIsSuspended="on"
  asyncWriting="off" flushOnTXEnd="on")
}
'
startup
test_error_exit_handler() {
    if [ -n "${producer_pid:-}" ]; then
        kill "$producer_pid" 2>/dev/null || true
        wait "$producer_pid" 2>/dev/null || true
    fi
}
tcpflood -m2 -i0 &
producer_pid=$!
injectmsg 2 2
wait "$producer_pid" || error_exit 1
producer_pid=''
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 4
shutdown_when_empty
wait_shutdown
seq_check
exit_test
