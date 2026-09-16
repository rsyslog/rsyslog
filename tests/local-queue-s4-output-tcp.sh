#!/bin/bash
# This file is part of rsyslog.
# Released under ASL 2.0.
# Real omfwd multitarget transactions from actual imtcp FE and imdiag BE work.
# Receiver 2 listens but gates all reads; receiver 1 must observe data before
# release. This establishes delayed receiver service, not a TCP delivery ACK or
# a guaranteed blocked sender. After release, exact union inventory at the two
# receivers proves delivery. A bounded rebind interval exercises worker-private
# disconnect/reconnect state; receivers stay alive across connection closes.
# Impstats proves both FE and BE ingress. Timeouts are hang watchdogs only.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
export NUMMESSAGES=256
STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
READ_RELEASE="$PWD/$RSYSLOG_DYNNAME.read-release"
KEEP="$PWD/$RSYSLOG_DYNNAME.keep"
STOPMARK="$PWD/$RSYSLOG_DYNNAME.stop"
touch "$KEEP"
export MINITCPSRV_EXTRA_OPTS="-K $KEEP"
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1
MINITCPSRV_EXTRA_OPTS="-K $KEEP -Q $READ_RELEASE"
start_minitcpsrvr "$RSYSLOG2_OUT_LOG" 2
unset MINITCPSRV_EXTRA_OPTS
generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="1024" queue.workerThreads="1"
 queue.dequeueBatchSize="16" queue.local.frontendSize="512" queue.local.maxFrontends="1")
template(name="wire" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then action(type="omfwd" protocol="tcp"
 target=["127.0.0.1", "127.0.0.1"] port=["'$MINITCPSRVR_PORT1'", "'$MINITCPSRVR_PORT2'"]
 template="wire" rebindInterval="64" pool.resumeInterval="1"
 action.resumeRetryCount="3" action.resumeInterval="1")
'
startup
test_error_exit_handler() {
    if [ -n "${producer_pid:-}" ]; then
        kill "$producer_pid" 2>/dev/null || true
        wait "$producer_pid" 2>/dev/null || true
    fi
}
tcpflood -m128 -i0 &
producer_pid=$!
injectmsg 128 128
wait "$producer_pid" || error_exit 1
producer_pid=''
wait_file_lines "$RSYSLOG_OUT_LOG" 1
localq_wait_stats_regex "$STATSFILE" 'main Q.local' 'route.fe.messages=[1-9][0-9]*' 'route.be.messages=[1-9][0-9]*'
touch "$READ_RELEASE"
# Counts per target may differ because each output WID has its own rotation.
# Wait on the combined receiver count, then check exact unique IDs after stop.
deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
while [ "$(date +%s)" -le "$deadline" ]; do
    received=$(cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" 2>/dev/null | wc -l)
    [ "$received" -ge "$NUMMESSAGES" ] && break
    "$TESTTOOL_DIR/msleep" 100
done
[ "$received" -eq "$NUMMESSAGES" ] || error_exit 1 'receiver inventory did not reach exact total'
wait_file_lines "$RSYSLOG2_OUT_LOG" 1
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 ;; esac
shutdown_when_empty
wait_shutdown
wait_file_lines "$STOPMARK" 1
export SEQ_CHECK_FILE="$PWD/$RSYSLOG_DYNNAME.combined"
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"
seq_check
rm -f "$KEEP"
exit_test
