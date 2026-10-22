#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# A classic DA action queue has one item held by its suspended worker while
# the 20-slot memory tier is full. Run both LinkedList and FixedArray memory
# tiers: an orderly SIGTERM switches workers to immediate shutdown, and
# saveOnShutdown must preserve the held item as well as every admitted queued
# item. Recovery must produce the exact admitted ID set for each tier.
#
# The testbench hook marks the DA transfer worker after it leaves the queue
# loop and holds it there until shutdown. Six seed messages trigger the first
# transfer; with that worker held, a fresh queue-size sample determines exactly
# how many more messages fill the primary tier. The reference action runs after
# each enqueue returns. All reference rows, a fresh full-size sample, and the
# active marker are required before SIGTERM. The testbench timeout only guards
# a stuck fixture; exact recovery is the pass/fail oracle.
if [ -z "${RSYSLOG_TESTBENCH_SHUTDOWN_QUEUE_TYPE:-}" ]; then
	RSYSLOG_TESTBENCH_SHUTDOWN_QUEUE_TYPE=LinkedList "$0" || exit $?
	RSYSLOG_TESTBENCH_SHUTDOWN_QUEUE_TYPE=FixedArray "$0" || exit $?
	exit 0
fi
. ${srcdir:=.}/diag.sh init
require_plugin impstats
export RSYSLOG_TESTBENCH_WORKER_EXIT_DELAY_POOL=":DAwpool"
export RSYSLOG_TESTBENCH_WORKER_EXIT_WAIT_FOR_SHUTDOWN=1
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
STATS_FILE="$PWD/${RSYSLOG_DYNNAME}.stats"
ACTIVE_FILE="$PWD/${RSYSLOG_DYNNAME}.da-exit-active"
export RSYSLOG_TESTBENCH_WORKER_EXIT_ACTIVE_FILE="$ACTIVE_FILE"
REFERENCE_FILE="$PWD/${RSYSLOG_DYNNAME}.reference.log"
RECOVERED_FILE="$PWD/${RSYSLOG_DYNNAME}.recovered.log"
BLOCKED_FILE="$PWD/${RSYSLOG_DYNNAME}.missing/output.log"

write_conf() {
	generate_conf
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'" processInternalMessages="off")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'"$STATS_FILE"'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then {
	action(type="omfile" file="'"$1"'" template="outfmt"
		createDirs="off" action.resumeRetryCount="-1"
		queue.type="'"$RSYSLOG_TESTBENCH_SHUTDOWN_QUEUE_TYPE"'" queue.filename="daq"
		queue.diskQueueType="disk" queue.size="20"
		queue.highWatermark="5" queue.lowWatermark="2"
		queue.dequeueBatchSize="1" queue.timeoutEnqueue="60000"
		queue.timeoutShutdown="100" queue.timeoutActionCompletion="100"
		queue.saveOnShutdown="on")
	# A reference row proves that this action enqueue returned.
	action(type="omfile" file="'"$REFERENCE_FILE"'" template="outfmt")
}
'
}

queue_stats_count() {
	if [ -f "$STATS_FILE" ]; then
		grep -Fc 'action-1-builtin:omfile queue: origin=core.queue ' "$STATS_FILE" || true
	else
		printf '0\n'
	fi
}

wait_fresh_primary_size() {
	local before="$1" deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) line count
	while [ "$(date +%s)" -le "$deadline" ]; do
		count=$(queue_stats_count)
		if [ "$count" -gt "$before" ]; then
			line=$(grep -F 'action-1-builtin:omfile queue: origin=core.queue ' "$STATS_FILE" | tail -n 1)
			if [[ " $line " =~ [[:space:]]size=([0-9]+)[[:space:]] ]]; then
				PRIMARY_SIZE="${BASH_REMATCH[1]}"
				return 0
			fi
			error_exit 1 "action queue stats did not contain a size: $line"
		fi
		"$TESTTOOL_DIR/msleep" 50
	done
	error_exit 1 'no fresh action queue stats sample while DA worker was held'
}

mkdir -p "$SPOOL_DIR"
rm -f "$ACTIVE_FILE"
write_conf "$BLOCKED_FILE"
startup
injectmsg 0 6
wait_file_lines "$REFERENCE_FILE" 6
wait_file_lines "$ACTIVE_FILE" 1
stats_before=$(queue_stats_count)
wait_fresh_primary_size "$stats_before"
if [ "$PRIMARY_SIZE" -lt 1 ] || [ "$PRIMARY_SIZE" -ge 20 ]; then
	error_exit 1 "unexpected primary size after seed transfer: $PRIMARY_SIZE"
fi
fill=$((20 - PRIMARY_SIZE))
export NUMMESSAGES=$((6 + fill))
injectmsg 6 "$fill"
wait_file_lines "$REFERENCE_FILE" "$NUMMESSAGES"
stats_before=$(queue_stats_count)
wait_fresh_primary_size "$stats_before"
[ "$PRIMARY_SIZE" -eq 20 ] || error_exit 1 "primary queue was not full before shutdown: $PRIMARY_SIZE"
[ -s "$ACTIVE_FILE" ] || error_exit 1 'DA transfer worker left its test barrier before shutdown'
shutdown_immediate
wait_shutdown

unset RSYSLOG_TESTBENCH_WORKER_EXIT_DELAY_POOL RSYSLOG_TESTBENCH_WORKER_EXIT_WAIT_FOR_SHUTDOWN
unset RSYSLOG_TESTBENCH_WORKER_EXIT_ACTIVE_FILE
write_conf "$RECOVERED_FILE"
startup
wait_file_lines --abort-on-oversize "$RECOVERED_FILE" "$NUMMESSAGES" 30
shutdown_when_empty
wait_shutdown
sort "$REFERENCE_FILE" > "${REFERENCE_FILE}.sorted"
sort "$RECOVERED_FILE" > "${RECOVERED_FILE}.sorted"
cmp_exact_file "${REFERENCE_FILE}.sorted" "${RECOVERED_FILE}.sorted"
rm -rf "$SPOOL_DIR"
exit_test
