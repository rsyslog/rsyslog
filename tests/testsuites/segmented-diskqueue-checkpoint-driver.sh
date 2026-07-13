#!/bin/bash
# Shared checkpoint-interval driver. A single large active segment prevents
# topology writes and dequeue batches contain one record. The oracle reads the
# durable slot generation directly before shutdown, so stats traffic cannot
# itself perturb the completion-checkpoint count.
. ${srcdir:=.}/diag.sh init
check_command_available python3
: "${CHECKPOINT_INTERVAL:?CHECKPOINT_INTERVAL is required}"
: "${EXPECTED_PERIODIC_WRITES:?EXPECTED_PERIODIC_WRITES is required}"
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
injectmsg
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
wait_queueempty
current=$(state_generation)
actual=$((current - baseline))
if [ "$actual" -ne "$EXPECTED_PERIODIC_WRITES" ]; then
	printf 'FAIL: checkpointInterval=%s produced %s periodic state writes, expected %s\n' \
		"$CHECKPOINT_INTERVAL" "$actual" "$EXPECTED_PERIODIC_WRITES"
	python3 "$srcdir/segdisk-inspect.py" "$STORE_DIR"
	error_exit 1
fi
shutdown_when_empty
wait_shutdown
seq_check
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
