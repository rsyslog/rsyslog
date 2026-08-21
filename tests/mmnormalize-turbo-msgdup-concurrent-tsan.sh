#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Two threads duplicate the same turbo-normalized smsg_t at the same time.
# Two omruleset actions with their own queues receive the message by
# reference (copyMsg=off) and each action worker calls MsgDup() on it to
# submit a copy to its target ruleset; the ruleset worker itself calls a
# queued ruleset, which is a third MsgDup() on the same source. This covers
# first publication of the shared snapshot counter from concurrent
# duplicators and shared snapshot release from the copies. ThreadSanitizer
# must stay silent and every target must receive every sequence number.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize
require_plugin omruleset

export NUMMESSAGES="${NUMMESSAGES:-5000}"
TSAN_LOG="$PWD/$RSYSLOG_DYNNAME.tsan"
export TSAN_OPTIONS="${TSAN_OPTIONS:+$TSAN_OPTIONS:}log_path=$TSAN_LOG"
if [[ "$TSAN_OPTIONS" != *"symbolize="* ]]; then
	export TSAN_OPTIONS="$TSAN_OPTIONS:symbolize=0"
fi

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/omruleset/.libs/omruleset")

main_queue(
	queue.type="FixedArray"
	queue.size="20000"
	queue.workerThreads="4"
	queue.workerThreadMinimumMessages="1"
	queue.dequeueBatchSize="1"
)

input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="normalize")

template(name="num" type="string" string="%$!num%\n")

ruleset(name="target1" queue.type="LinkedList" queue.workerThreads="2") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num" flushOnTXEnd="on")
}
ruleset(name="target2" queue.type="LinkedList" queue.workerThreads="2") {
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="num" flushOnTXEnd="on")
}
ruleset(name="copy" queue.type="LinkedList" queue.workerThreads="2") {
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.copy.log" template="num" flushOnTXEnd="on")
}

$ruleset normalize
action(type="mmnormalize" turbo="on" rule="rule=:msgnum:%num:number%:")
$ActionQueueType LinkedList
$ActionQueueWorkerThreads 2
$ActionQueueDequeueBatchSize 1
$ActionOmrulesetRulesetName target1
*.* :omruleset:
$ActionQueueType LinkedList
$ActionQueueWorkerThreads 2
$ActionQueueDequeueBatchSize 1
$ActionOmrulesetRulesetName target2
*.* :omruleset:
call copy
'

dump_tsan_report() {
	if [ -n "${TSAN_REPORT:-}" ] && [ -s "$TSAN_REPORT" ]; then
		printf 'ThreadSanitizer report from rsyslogd:\n'
		cat "$TSAN_REPORT"
	fi
}

startup
TSAN_REPORT="$TSAN_LOG.$(cat "$RSYSLOG_PIDBASE.pid")"
trap dump_tsan_report EXIT
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
if [ -s "$TSAN_REPORT" ]; then
	printf 'ThreadSanitizer report detected\n'
	error_exit 1
fi
seq_check 0 $((NUMMESSAGES - 1))
seq_check2 0 $((NUMMESSAGES - 1))
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.copy.log"
seq_check 0 $((NUMMESSAGES - 1))
exit_test
