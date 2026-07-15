#!/bin/bash
# Verify the two boundary values of queue.diskQueueIdleTimeout. Identical slow
# action queues force a spill; timeout 0 must remove its store immediately,
# while -1 must retain its drained store and final worker until shutdown. Exact
# output sequences prove both children drained without loss.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=1500
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
ZERO_OUT="${RSYSLOG_DYNNAME}.zero.log"
DISABLED_OUT="${RSYSLOG_DYNNAME}.disabled.log"
STATS_FILE="${RSYSLOG_DYNNAME}.stats"

wait_path_state() {
	local mode="$1" path="$2" timeoutend=$((SECONDS + 30))
	while true; do
		[ "$mode" = present ] && [ -e "$path" ] && return
		[ "$mode" = absent ] && [ ! -e "$path" ] && return
		[ "$SECONDS" -ge "$timeoutend" ] && error_exit 1 "path did not become $mode: $path"
		./msleep 25
	done
}

generate_conf
add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
module(load="../plugins/impstats/.libs/impstats"
	interval="1" log.syslog="off" log.file="'"$STATS_FILE"'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

:msg, contains, "msgnum:" {
	action(type="omfile" name="idle-zero" file="'"$ZERO_OUT"'" template="outfmt"
		queue.type="LinkedList" queue.filename="idle-zero" queue.size="50"
		queue.highWatermark="10" queue.lowWatermark="5"
		queue.dequeueBatchSize="1" queue.dequeueSlowdown="10"
		queue.diskQueueType="segmentedDisk" queue.diskQueueIdleTimeout="0")
	action(type="omfile" name="idle-disabled" file="'"$DISABLED_OUT"'" template="outfmt"
		queue.type="LinkedList" queue.filename="idle-disabled" queue.size="50"
		queue.highWatermark="10" queue.lowWatermark="5"
		queue.dequeueBatchSize="1" queue.dequeueSlowdown="10"
		queue.workerThreads="3" queue.workerThreadMinimumMessages="1"
		queue.timeoutWorkerThreadShutdown="100"
		queue.diskQueueType="segmentedDisk" queue.diskQueueIdleTimeout="-1")
}
'

startup
injectmsg
wait_path_state present "$SPOOL_DIR/idle-disabled.segq"
wait_file_lines "$ZERO_OUT" "$NUMMESSAGES" 300
wait_file_lines "$DISABLED_OUT" "$NUMMESSAGES" 300
wait_path_state absent "$SPOOL_DIR/idle-zero.segq"
./msleep 1000
[ -d "$SPOOL_DIR/idle-disabled.segq" ] || error_exit 1 "idle timeout -1 removed the store"
# With a zero grace period, a producer scheduling gap may permit more than one
# drain/rematerialize cycle during the burst. At least one completed cleanup,
# an absent store, and zero child workers are the deterministic immediacy
# oracle; requiring exactly one cleanup would make scheduler timing observable.
wait_content 'store.idleDematerializations=' "$STATS_FILE"
zero_dematerializations=$(grep 'idle-zero queue\[DA\]' "$STATS_FILE" |
	sed -E 's/.*store\.idleDematerializations=([0-9]+).*/\1/' | sort -n | tail -n 1)
[ "${zero_dematerializations:-0}" -ge 1 ] || error_exit 1 "idle timeout 0 did not dematerialize its store"
content_check --regex 'idle-zero queue\[DA\].*workers.current=0' "$STATS_FILE"
# The 100ms generic worker timeout has elapsed several times by now.  A live
# worker proves that -1 protects slot zero while still leaving the generic
# timeout available for any additional workers.
content_check --regex 'idle-disabled queue\[DA\].*workers.current=1' "$STATS_FILE"

shutdown_when_empty
wait_shutdown
RSYSLOG_OUT_LOG="$ZERO_OUT" seq_check 0 $((NUMMESSAGES - 1))
RSYSLOG_OUT_LOG="$DISABLED_OUT" seq_check 0 $((NUMMESSAGES - 1))
exit_test
