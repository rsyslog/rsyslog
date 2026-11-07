#!/bin/bash
# Verify the lifetime FE-registration bound falls back to BE. Two independent
# imtcp listeners each own one actual worker context. The first listener claims
# the sole FE and its callback is held by a FIFO before the second listener is
# given input. Thus the second producer must exhaust the registration bound,
# independent of how a shared worker pool would schedule TCP connections.
# Require one registered/started FE and a positive capacity-exhausted BE route;
# exact IDs after release prove fallback neither lost nor duplicated a batch.
# The reserved bound stays B+N*(F+D)=1024+1*(8+4)=1036.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
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
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.second_port" workerThreads="1")
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
tcpflood -m1 -i0
wait_file_lines "$ENTERFILE" 1
localq_wait_stats "$STATSFILE" "main Q.local" "fe.registered=1" "fe.started=1"
assign_tcpflood_port "${RSYSLOG_DYNNAME}.second_port"
tcpflood -m399 -i1
localq_wait_stats_regex "$STATSFILE" "main Q.local" \
	"fe.registered=1" "fe.started=1" "route.be.reason.capacity_exhausted.messages=[1-9][0-9]*" \
	"resource.reserved.messages=1036" "resource.be.capacity.messages=1024" \
	"resource.fe.capacity.messages=8" "resource.fe.active.capacity.messages=4"
localq_release_barrier "$RELEASEFIFO"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
