#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# A turbo-only message is shared by reference with an asynchronous action
# that reads a single field (%$!num%) while the continuing ruleset runs a
# set statement. set materializes the snapshot into the JSON tree and then
# mutates it under the message mutex; the action worker's field read must
# be synchronized with that NULL-to-tree transition. ThreadSanitizer is the
# detector. The oracle after the fix: no report, every sequence number in the
# shared-action output (the field exists in both states), and every record
# in the direct output carries both the field and the set key.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

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
template(name="numk" type="string" string="%$!num%-%$!k%\n")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num"
		action.copyMsg="off"
		queue.type="LinkedList" queue.workerThreads="4"
		queue.dequeueBatchSize="1" queue.workerThreadMinimumMessages="1"
		flushOnTXEnd="on")
	# a global variable write runs under the global-variables mutex, not the
	# message mutex: it must not touch the snapshot of the shared message
	set $/g = "g";
	set $!k = "v";
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="numk")
}
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
withk=$(grep -c -E '^[0-9]+-v$' "$RSYSLOG2_OUT_LOG")
if [ "$withk" -ne "$NUMMESSAGES" ]; then
	printf 'only %s of %s direct records carry num and k\n' "$withk" "$NUMMESSAGES"
	error_exit 1
fi
sed -E 's/^([0-9]+)-v$/\1/' "$RSYSLOG2_OUT_LOG" > "$RSYSLOG_DYNNAME.num.log"
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.num.log"
seq_check 0 $((NUMMESSAGES - 1))
exit_test
