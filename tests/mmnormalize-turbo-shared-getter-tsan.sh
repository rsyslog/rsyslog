#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# A turbo-normalized message is shared by reference with an asynchronous
# action (default copyMsg=off, so the action queue holds the same smsg_t via
# MsgAddRef) while the continuing ruleset runs a second turbo mmnormalize
# action on it. The second normalization releases the first snapshot and
# attaches a new one while the action worker may still be inside the $!num
# property getter. Under ThreadSanitizer an unsynchronized getter reports a
# race on the snapshot slots; under AddressSanitizer it can read freed
# snapshot memory. The post-fix oracle is: no sanitizer report, every
# sequence number present in the shared-action output, and every sequence
# number present in the direct output written after the second
# normalization. Both rules extract the same num value, so the shared
# reader's per-record value is deterministic whichever snapshot it sees.
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

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	# shared by reference with the action queue worker, which reads $!num
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num"
		action.copyMsg="off"
		queue.type="LinkedList" queue.workerThreads="4"
		queue.dequeueBatchSize="1" queue.workerThreadMinimumMessages="1"
		flushOnTXEnd="on")
	# second normalization: releases the first snapshot, attaches another
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:char-to:\\x3a%:")
	# script-level read of the field after the overwrite
	if $!num == "" then {
		action(type="omfile" file="'$RSYSLOG_DYNNAME'.empty.log" template="num")
	}
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="num")
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
if [ -s "$RSYSLOG_DYNNAME.empty.log" ]; then
	printf 'second normalization lost $!num for %s records\n' \
		"$(wc -l < "$RSYSLOG_DYNNAME.empty.log")"
	error_exit 1
fi
seq_check 0 $((NUMMESSAGES - 1))
seq_check2 0 $((NUMMESSAGES - 1))
exit_test
