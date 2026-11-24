#!/bin/bash
# Verify the lifetime FE-registration bound with two actual live producers.
# The imdiag prime fixture publishes singleton ID0 through normal local queue
# admission on its long-lived input thread, pinning the sole FE behind a FIFO.
# TCP traffic then comes from a distinct input thread and must take the exact
# capacity-exhausted BE route, even when one imtcp worker handles all sockets.
# This also works with the portable poll path, which uses one imtcp worker.
# One FE message, 399 capacity fallbacks, the live FE producer, and the fixed
# bound prove pressure without using connection counts as identity. All 400 IDs
# after release prove no loss or duplication; B+N*(F+D)=1024+1*(8+4)=1036.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats
require_plugin omtesting

export NUMMESSAGES=400
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
ENTERFILE="$PWD/${RSYSLOG_DYNNAME}.entered"
RELEASEFIFO="$PWD/${RSYSLOG_DYNNAME}.release"
mkfifo "$RELEASEFIFO"

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="2")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="1024"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="4"
	queue.local.frontendSize="8" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTERFILE' '$RELEASEFIFO';localqfmt
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup
response=$(printf 'localqueuesubmittest prime\n' | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in *OK*) ;; *) echo "FAIL: local prime response: $response"; error_exit 1 ;; esac
wait_file_lines "$ENTERFILE" 1
localq_wait_stats "$STATSFILE" "main Q.local" \
	"fe.registered=1" "fe.started=1" "fe.producerless=0" "fe.inflight.messages=1" "route.fe.messages=1"
tcpflood -c2 -m399 -i1
localq_wait_stats "$STATSFILE" "main Q.local" \
	"fe.registered=1" "fe.started=1" "fe.producerless=0" "route.fe.messages=1" \
	"fe.inflight.messages=1" "route.be.reason.capacity_exhausted.messages=399" \
	"resource.reserved.messages=1036" "resource.be.capacity.messages=1024" \
	"resource.fe.capacity.messages=8" "resource.fe.active.capacity.messages=4"
localq_release_barrier "$RELEASEFIFO"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
