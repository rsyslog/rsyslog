#!/bin/bash
# Shared pure disk-assisted queue behavior driver. The wrapper selects one
# engine, memory queue type, and placement (main, ruleset, or action).
# A tiny queue with single-record, slowed dequeue must spill a 2000-message
# burst. The engine-specific filesystem object proves the child was used; the
# exact 0..1999 output sequence proves lossless ordered spill and drain.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=2000
DA_SCOPE=${DA_SCOPE:?DA_SCOPE must be main, ruleset, or action}
DA_ENGINE=${DA_ENGINE:?DA_ENGINE must be auto or disk}
DA_QUEUE_TYPE=${DA_QUEUE_TYPE:?DA_QUEUE_TYPE must be LinkedList or FixedArray}
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"

wait_for_path() {
	local path="$1"
	local deadline=$((SECONDS + 30))
	while [ ! -e "$path" ]; do
		if [ "$SECONDS" -ge "$deadline" ]; then
			find "$SPOOL_DIR" -maxdepth 2 -print 2>/dev/null || true
			error_exit 1 "$DA_SCOPE DA child did not create $path"
		fi
		./msleep 25
	done
}

wait_for_classic_segment() {
	local deadline=$((SECONDS + 30))
	while ! compgen -G "$SPOOL_DIR/scopeq.[0-9]*" >/dev/null; do
		if [ "$SECONDS" -ge "$deadline" ]; then
			find "$SPOOL_DIR" -maxdepth 1 -print 2>/dev/null || true
			error_exit 1 "$DA_SCOPE classic DA child did not create a numeric segment"
		fi
		./msleep 25
	done
}

if [ "$DA_ENGINE" = auto ]; then
	ENGINE_CONFIG='queue.diskQueueType="auto"
	queue.diskQueueIdleTimeout="-1"'
	EXPECTED_ENGINE=segmentedDisk
else
	ENGINE_CONFIG='queue.diskQueueType="disk"'
	EXPECTED_ENGINE=disk
fi

QUEUE_CONFIG='queue.type="'"$DA_QUEUE_TYPE"'"
	queue.filename="scopeq"
	queue.size="50"
	queue.highWatermark="10"
	queue.lowWatermark="5"
	queue.dequeueBatchSize="1"
	queue.dequeueSlowdown="2"
	'"$ENGINE_CONFIG"

generate_conf
case "$DA_SCOPE" in
main)
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
'"$QUEUE_CONFIG"'
)
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
	;;
ruleset)
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="queued"
'"$QUEUE_CONFIG"'
) {
	action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
}
:msg, contains, "msgnum:" call queued
'
	;;
action)
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(
	type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt"
'"$QUEUE_CONFIG"'
)
'
	;;
*)
	error_exit 1 "unknown DA_SCOPE: $DA_SCOPE"
	;;
esac

startup
injectmsg

MARKER="$SPOOL_DIR/scopeq.da-engine"
wait_for_path "$MARKER"
[ "$(cat "$MARKER")" = "RSYSLOG-DA-ENGINE-V1 $EXPECTED_ENGINE" ] ||
	error_exit 1 "$DA_SCOPE DA engine marker selected the wrong engine"
if [ "$DA_ENGINE" = auto ]; then
	wait_for_path "$SPOOL_DIR/scopeq.segq"
else
	wait_for_classic_segment
fi

wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" 300
shutdown_when_empty
wait_shutdown
seq_check 0 1999
exit_test
