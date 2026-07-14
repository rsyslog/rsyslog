#!/bin/bash
# Shared idle-cleanup crash driver for a segmented disk-assisted main queue.
# A small, slow memory tier guarantees a spill. The configured imdiag hook must
# SIGKILL rsyslog during empty-store cleanup; restart must accept the surviving
# marker/state combination and preserve the exact acknowledged sequence.
. ${srcdir:=.}/diag.sh init
: "${SEGDISK_IDLE_FAULT_POINT:?SEGDISK_IDLE_FAULT_POINT is required}"
SEGDISK_IDLE_FAULT_REPEATS=${SEGDISK_IDLE_FAULT_REPEATS:-1}
export NUMMESSAGES=$((1000 * SEGDISK_IDLE_FAULT_REPEATS))
export RSTB_IMDIAG_INJECT_DELAY_MODE=none
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.diskQueueType="segmentedDisk" queue.diskQueueIdleTimeout="0"
	queue.size="50" queue.highWatermark="10" queue.lowWatermark="5"
	queue.dequeueBatchSize="1" queue.dequeueSlowdown="2"
	queue.checkpointInterval="1" queue.saveOnShutdown="on")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

for ((cycle = 0; cycle < SEGDISK_IDLE_FAULT_REPEATS; ++cycle)); do
	write_conf
	startup
	set_segmented_disk_fault "$SEGDISK_IDLE_FAULT_POINT"
	export RSTB_EXPECT_NO_PROPER_TERMINATION=YES
	crashed_pid=$(getpid)
	# Every cycle appends a distinct range. This rematerializes the store if an
	# earlier interrupted cleanup completed during restart and proves the armed
	# fault state survives the unmaterialized child representation.
	injectmsg $((cycle * 1000)) 1000 || true
	for _ in {1..600}; do
		kill -0 "$crashed_pid" 2>/dev/null || break
		./msleep 100
	done
	if kill -0 "$crashed_pid" 2>/dev/null; then
		error_exit 1 "idle fault point did not terminate rsyslogd: $SEGDISK_IDLE_FAULT_POINT"
	fi
	wait "$crashed_pid" 2>/dev/null || true
	rm -f "$RSYSLOG_PIDBASE.pid"
	unset RSTB_EXPECT_NO_PROPER_TERMINATION
done

write_conf
startup
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1))
[ -f "$SPOOL_DIR/mainq.da-engine" ] || error_exit 1 "engine marker missing after cleanup crash"
exit_test
