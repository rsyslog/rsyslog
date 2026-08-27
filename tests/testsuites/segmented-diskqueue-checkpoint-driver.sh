#!/bin/bash
# Shared checkpoint-interval driver. A single large active segment prevents
# topology writes and dequeue batches contain one record. The oracle reads the
# durable slot generation directly before shutdown, so stats traffic cannot
# itself perturb the completion-checkpoint count. Startup records are part of
# the queue workload, so the oracle includes their interval residue instead of
# assuming injected records begin at a checkpoint boundary.
. ${srcdir:=.}/diag.sh init
check_command_available python3
: "${CHECKPOINT_INTERVAL:?CHECKPOINT_INTERVAL is required}"
export NUMMESSAGES=12
STORE_DIR="${RSYSLOG_DYNNAME}.spool/mainq.segq"

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="16m" queue.dequeueBatchSize="1"
	queue.workerThreads="1" queue.checkpointInterval="'$CHECKPOINT_INTERVAL'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

state_generation() {
	python3 "$srcdir/segdisk-inspect.py" "$STORE_DIR" --state-generation
}

startup
wait_queueempty
baseline=$(state_generation)
baseline_records=$(python3 "$srcdir/segdisk-inspect.py" "$STORE_DIR" --record-count)
injectmsg
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
wait_queueempty
current=$(state_generation)
actual=$((current - baseline))
expected=0
if [ "$CHECKPOINT_INTERVAL" -gt 0 ]; then
	expected=$(( (baseline_records + NUMMESSAGES) / CHECKPOINT_INTERVAL - baseline_records / CHECKPOINT_INTERVAL ))
fi
if [ "$actual" -ne "$expected" ]; then
	printf 'FAIL: checkpointInterval=%s produced %s periodic state writes, expected %s (baseline records=%s)\n' \
		"$CHECKPOINT_INTERVAL" "$actual" "$expected" "$baseline_records"
	python3 "$srcdir/segdisk-inspect.py" "$STORE_DIR"
	error_exit 1
fi
shutdown_when_empty
wait_shutdown
seq_check
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
