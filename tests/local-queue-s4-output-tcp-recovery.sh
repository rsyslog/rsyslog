#!/bin/bash
# A real omfwd pool starts with target 2 bound but not listening. Target 1 must
# receive phase 1 completely. After target 2 reaches listen(), a new FE batch
# must reach it and the exact receiver union must contain each ID once. The
# only clock wait crosses omfwd's whole-second pool retry eligibility; acceptance
# and actual received records, rather than elapsed time, prove recovery.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
export NUMMESSAGES=64
KEEP="$PWD/$RSYSLOG_DYNNAME.keep"
LISTEN_RELEASE="$PWD/$RSYSLOG_DYNNAME.listen-release"
LISTEN_READY="$PWD/$RSYSLOG_DYNNAME.listen-ready"
ACCEPT_READY="$PWD/$RSYSLOG_DYNNAME.accept-ready"
touch "$KEEP"
export MINITCPSRV_EXTRA_OPTS="-K $KEEP"
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1
start_minitcpsrvr "$RSYSLOG2_OUT_LOG" 2 "$LISTEN_RELEASE" "$LISTEN_READY" "$ACCEPT_READY"
unset MINITCPSRV_EXTRA_OPTS
generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="128" queue.workerThreads="1"
 queue.dequeueBatchSize="8" queue.local.frontendSize="128" queue.local.maxFrontends="1")
template(name="wire" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then action(type="omfwd" protocol="tcp"
 target=["127.0.0.1", "127.0.0.1"] port=["'$MINITCPSRVR_PORT1'", "'$MINITCPSRVR_PORT2'"]
 template="wire" pool.resumeInterval="1" action.resumeRetryCount="3" action.resumeInterval="1")
'
startup
tcpflood -m32 -i0
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 32
touch "$LISTEN_RELEASE"
wait_file_exists "$LISTEN_READY"
retry_after=$(( $(date +%s) + 2 ))
while [ "$(date +%s)" -lt "$retry_after" ]; do "$TESTTOOL_DIR/msleep" 100; done
tcpflood -m32 -i32
wait_file_exists "$ACCEPT_READY"
wait_file_lines --abort-on-oversize "$RSYSLOG2_OUT_LOG" 16
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 48
shutdown_when_empty
wait_shutdown
export SEQ_CHECK_FILE="$PWD/$RSYSLOG_DYNNAME.combined"
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"
seq_check
rm -f "$KEEP"
exit_test
