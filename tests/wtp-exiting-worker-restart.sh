#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Regression test for a lost worker start in single-slot worker pools.
#
# A classic disk-assisted (DA) queue has exactly one transfer worker, and that
# worker retires whenever the in-memory part drains to the low watermark. After
# it leaves its queue loop but before its slot is joinable, the slot can be
# neither started nor signalled. Start advice issued in that window used to be
# dropped; a producer that then found the queue full waited its whole enqueue
# timeout and discarded one message per occurrence (seen as a rare 30s stall
# in segmented-da-event-properties.sh on CI).
#
# The testbench-only RSYSLOG_TESTBENCH_WORKER_EXIT_DELAY_* hook holds the DA
# pool's exiting worker in that window for 1s. The action is suspended
# (missing parent directory, infinite retries), so only DA transfer can make
# room; with dequeueBatchSize 1 the suspended regular worker pins exactly one
# message, so DA transfer drains to the low watermark and retires while the
# main-queue worker is still enqueuing. That producer refills the 20-slot
# queue within microseconds, i.e. inside the widened window, on every DA
# cycle. With the fix the exiting worker resumes after the delay; the
# producer waits about 1s per cycle, well below queue.timeoutEnqueue (10s).
# The oracle is lossless persistence: all messages must survive into the disk
# queue and be delivered after restart. With the bug, every cycle loses its
# advice, the producer times out and discards a message, so the recovered
# file stays short and the comparison fails.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100
export RSYSLOG_TESTBENCH_WORKER_EXIT_DELAY_POOL=":DAwpool"
export RSYSLOG_TESTBENCH_WORKER_EXIT_DELAY_MS=1000
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
REFERENCE_FILE="$PWD/${RSYSLOG_DYNNAME}.reference.log"
RECOVERED_FILE="$PWD/${RSYSLOG_DYNNAME}.recovered.log"
BLOCKED_FILE="$PWD/${RSYSLOG_DYNNAME}.missing/output.log"

write_conf() {
	generate_conf
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	action(type="omfile" file="'"$REFERENCE_FILE"'" template="outfmt")
	action(type="omfile" file="'"$1"'" template="outfmt"
		createDirs="off"
		action.resumeRetryCount="-1"
		queue.type="LinkedList"
		queue.filename="daq"
		queue.diskQueueType="disk"
		queue.size="20"
		queue.highWatermark="5"
		queue.lowWatermark="2"
		queue.dequeueBatchSize="1"
		queue.timeoutEnqueue="10000"
		queue.saveOnShutdown="on")
}
'
}

mkdir -p "$SPOOL_DIR"
write_conf "$BLOCKED_FILE"
startup
injectmsg 0 "$NUMMESSAGES"
wait_file_lines "$REFERENCE_FILE" "$NUMMESSAGES"
shutdown_immediate
wait_shutdown

unset RSYSLOG_TESTBENCH_WORKER_EXIT_DELAY_POOL RSYSLOG_TESTBENCH_WORKER_EXIT_DELAY_MS
write_conf "$RECOVERED_FILE"
startup
wait_file_lines "$RECOVERED_FILE" "$NUMMESSAGES" 60
shutdown_when_empty
wait_shutdown
# DA shutdown may append in-memory records after already spilled ones;
# compare as sets so this test stays about loss, not ordering.
sort "$REFERENCE_FILE" > "${REFERENCE_FILE}.sorted"
sort "$RECOVERED_FILE" > "${RECOVERED_FILE}.sorted"
cmp_exact_file "${REFERENCE_FILE}.sorted" "${RECOVERED_FILE}.sorted"
rm -rf "$SPOOL_DIR"
exit_test
