#!/bin/bash
# Verify an actual INTERNAL_MSG enters the local queue's BE. Two concurrent
# imtcp submissions meet omtesting's existing two-callback barrier; its ordinary
# LogError emits one internal marker. The runtime's internal-route counter is
# the oracle, while a qualified Direct marker file proves the internal message
# also traversed the configured script. The test never assigns a connection to
# an actual producer: only the barrier's two real callbacks release the phase.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats
require_plugin omtesting

STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
INTERNAL_OUT="$PWD/${RSYSLOG_DYNNAME}.internal"
NORMAL_OUT="$PWD/${RSYSLOG_DYNNAME}.normal"

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="2")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.local.frontendSize="8" queue.local.maxFrontends="2" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then :omtesting:barrier_error 2 ;localqfmt
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$NORMAL_OUT'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
if ($msg contains "omtesting synchronized error") then
	action(type="omfile" file="'$INTERNAL_OUT'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup
tcpflood -m1 -i0 &
sender_a=$!
tcpflood -m1 -i1 &
sender_b=$!
wait "$sender_a" || error_exit $?
wait "$sender_b" || error_exit $?

localq_wait_stats "$STATSFILE" "main Q.local" "route.be.reason.internal.messages=1"
wait_file_lines --abort-on-oversize "$NORMAL_OUT" 2
wait_file_lines --abort-on-oversize "$INTERNAL_OUT" 1
shutdown_when_empty
wait_shutdown
exit_test
