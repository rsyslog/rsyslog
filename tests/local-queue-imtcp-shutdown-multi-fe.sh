#!/bin/bash
# Verify a local family requests cancellation for every blocked FE before it
# waits for any one cleanup. Two real imtcp callbacks enter one controlled
# omtesting action and block in cancellation points; this proves two actual FE
# callbacks without assigning either TCP connection an FE identity. On shutdown
# the first cleanup publishes FIRST and holds a FIFO. SECOND must publish before
# that FIFO is released, which is impossible for the former sequential
# cancel-and-join loop. The armed runtime final snapshot then proves both FEs
# joined STOPPED and all two accepted obligations became terminal/discarded;
# wait_shutdown is the bounded daemon watchdog and clean-termination oracle.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats
require_plugin omtesting

STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
ENTERFILE="$PWD/${RSYSLOG_DYNNAME}.entered"
FIRST_CLEANUP="$PWD/${RSYSLOG_DYNNAME}.first-cleanup"
SECOND_CLEANUP="$PWD/${RSYSLOG_DYNNAME}.second-cleanup"
CANCEL_BLOCK="$PWD/${RSYSLOG_DYNNAME}.cancel-block"
CLEANUP_RELEASE="$PWD/${RSYSLOG_DYNNAME}.cleanup-release"
STOPPED_MARKER="$PWD/${RSYSLOG_DYNNAME}.stopped"
mkfifo "$CANCEL_BLOCK" "$CLEANUP_RELEASE"

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="2")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.timeoutShutdown="1" queue.timeoutActionCompletion="1"
	queue.local.frontendSize="4" queue.local.maxFrontends="2" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg%\n")
if ($msg contains "localq-cancel") then
	:omtesting:cancel_cleanup_barrier '$ENTERFILE' '$CANCEL_BLOCK' '$FIRST_CLEANUP' '$SECOND_CLEANUP' '$CLEANUP_RELEASE';localqfmt
'
startup

tcpflood -m1 -M'localq-cancel' &
sender_a=$!
tcpflood -m1 -M'localq-cancel' &
sender_b=$!
wait_file_lines "$ENTERFILE" 2
wait "$sender_a" || error_exit $?
wait "$sender_b" || error_exit $?
localq_wait_stats "$STATSFILE" "main Q.local" \
	"fe.registered=2" "fe.started=2" "fe.inflight.messages=2" "outstanding.messages=2"

# Arm the ENABLE_TESTBENCH final snapshot before shutdown destroys the adapter.
# The command records a marker only after both FE states are joined/stopped and
# the accepted/terminal obligation totals reconcile.
response=$(printf "localqueuestopcheck %s\n" "$STOPPED_MARKER" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: local queue stop-check arm response: $response"
	error_exit 1
fi

shutdown_immediate
wait_file_lines "$FIRST_CLEANUP" 1
wait_file_lines "$SECOND_CLEANUP" 1
localq_release_barrier "$CLEANUP_RELEASE"
wait_shutdown
wait_file_lines "$STOPPED_MARKER" 1
exit_test
