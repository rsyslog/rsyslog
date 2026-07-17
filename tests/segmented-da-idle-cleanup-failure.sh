#!/bin/bash
# Force idle cleanup to encounter an unexpected file in the private store
# directory. The first cleanup must fail visibly while restoring a valid empty
# materialized store and keeping its worker alive. The restored state must
# reference one newly created active segment, proving partially removed old
# topology cannot be stranded beside the replacement state. Removing the
# blocker must allow the next complete idle interval to dematerialize normally.
# The 30-second path waits are event-driven; the generous bound only prevents
# a hung cleanup worker from stalling overloaded parallel CI indefinitely.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
SEG_DIR="$SPOOL_DIR/cleanup-failure.segq"
STATS_FILE="${RSYSLOG_DYNNAME}.stats"

wait_path() {
	local mode="$1" path="$2" timeoutend=$((SECONDS + 30))
	while true; do
		[ "$mode" = present ] && [ -e "$path" ] && return
		[ "$mode" = absent ] && [ ! -e "$path" ] && return
		[ "$SECONDS" -ge "$timeoutend" ] && error_exit 1 "path did not become $mode: $path"
		./msleep 50
	done
}

generate_conf
add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
module(load="../plugins/impstats/.libs/impstats"
	interval="1" log.syslog="off" log.file="'"$STATS_FILE"'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" name="cleanup-failure"
	file="'"$RSYSLOG_OUT_LOG"'" template="outfmt"
	queue.type="LinkedList" queue.filename="cleanup-failure"
	queue.size="50" queue.highWatermark="10" queue.lowWatermark="5"
	queue.dequeueBatchSize="1" queue.dequeueSlowdown="5"
	queue.diskQueueType="segmentedDisk" queue.diskQueueIdleTimeout="200")
'

startup
injectmsg
wait_path present "$SEG_DIR"
touch "$SEG_DIR/blocker"
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" 300
wait_content 'store.idleCleanupFailures=1' "$STATS_FILE"
wait_path present "$SEG_DIR/state"
find "$SEG_DIR" -maxdepth 1 -name '*.open' -print -quit | grep -q . ||
	error_exit 1 "cleanup failure did not restore an active segment"
segment_count=$(find "$SEG_DIR" -maxdepth 1 -type f -name 'segment-*' -print | wc -l)
[ "$segment_count" -eq 1 ] ||
	error_exit 1 "cleanup failure retained $segment_count segments instead of one replacement active segment"
rm -f "$SEG_DIR/blocker"
wait_path absent "$SEG_DIR"
wait_content 'store.idleDematerializations=1' "$STATS_FILE"

shutdown_when_empty
wait_shutdown
seq_check
exit_test
