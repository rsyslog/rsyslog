#!/bin/bash
# Exercise the complete segmented DA idle lifecycle on an action queue. A
# deliberately slow, tiny memory tier makes each 2000-message burst spill. The
# filesystem oracle proves lazy creation, grace-period dematerialization, and
# transparent rematerialization; exact sequence output proves no loss or
# reordering. The impstats oracle proves the final child worker terminated.
# The reset probe uses a 3-second timeout, injects after 1 second, and checks
# 2.25 seconds later: the check is past the original deadline but still 750 ms
# before the reset deadline, leaving a substantial scheduler margin under SAN.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=4000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
SEG_DIR="$SPOOL_DIR/idle-action.segq"
MARKER="$SPOOL_DIR/idle-action.da-engine"
STATS_FILE="${RSYSLOG_DYNNAME}.stats"

wait_path_state() {
	local mode="$1"
	local path="$2"
	local timeoutend=$((SECONDS + 30))
	while true; do
		if [ "$mode" = present ] && [ -e "$path" ]; then
			return
		fi
		if [ "$mode" = absent ] && [ ! -e "$path" ]; then
			return
		fi
		if [ "$SECONDS" -ge "$timeoutend" ]; then
			echo "FAIL: path did not become $mode: $path"
			find "$SPOOL_DIR" -maxdepth 2 -print 2>/dev/null || true
			error_exit 1
		fi
		./msleep 50
	done
}

generate_conf
add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
module(load="../plugins/impstats/.libs/impstats"
	interval="1" log.syslog="off" log.file="'"$STATS_FILE"'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(
	type="omfile"
	name="idle-action"
	file="'"$RSYSLOG_OUT_LOG"'"
	template="outfmt"
	queue.type="LinkedList"
	queue.filename="idle-action"
	queue.size="50"
	queue.highWatermark="10"
	queue.lowWatermark="5"
	queue.dequeueBatchSize="1"
	queue.dequeueSlowdown="2"
	queue.diskQueueType="segmentedDisk"
	queue.diskQueueIdleTimeout="3000"
)
'

startup
[ ! -e "$SEG_DIR" ] || error_exit 1 "segmented DA store materialized before the first spill"
[ ! -e "$MARKER" ] || error_exit 1 "segmented DA engine marker was created before the first spill"

injectmsg 0 2000
wait_path_state present "$SEG_DIR"
[ -f "$MARKER" ] || error_exit 1 "segmented DA engine marker was not created with the first spill"
wait_file_lines "$RSYSLOG_OUT_LOG" 2000 300

# A memory-tier-only enqueue during the grace period must wake the child and
# restart the full timeout even though it does not itself spill to disk.
./msleep 1000
injectmsg 2000 1
wait_file_lines "$RSYSLOG_OUT_LOG" 2001 300
./msleep 2250
[ -d "$SEG_DIR" ] || error_exit 1 "new traffic did not reset the segmented DA idle grace period"
wait_path_state absent "$SEG_DIR"

# A second burst must recreate the same child store and run through the same
# lifecycle, proving that worker and store teardown are both reversible.
injectmsg 2001 1999
wait_path_state present "$SEG_DIR"
wait_file_lines "$RSYSLOG_OUT_LOG" 4000 300
wait_path_state absent "$SEG_DIR"

wait_content 'store.idleDematerializations=2' "$STATS_FILE"
wait_content 'workers.current=0' "$STATS_FILE"
[ "$(cat "$MARKER")" = "RSYSLOG-DA-ENGINE-V1 segmentedDisk" ] ||
	error_exit 1 "segmented DA marker changed during idle cleanup"

shutdown_when_empty
wait_shutdown
seq_check 0 3999
exit_test
