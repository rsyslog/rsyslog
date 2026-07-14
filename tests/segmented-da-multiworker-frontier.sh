#!/bin/bash
# Seed a pure segmented queue, crash it with twelve uncommitted events, then
# reopen that store as an automatic segmented DA child with three one-record
# workers. Workers 1 and 2 hold events 0 and 1 for five and two seconds, so
# worker 3 must emit event 2. State inspection proves the durable frontier stays
# behind the held batch and then crosses the already completed suffix. Sorted
# exact output is the loss/duplicate oracle because worker output order is not
# otherwise constrained.
. ${srcdir:=.}/diag.sh init
check_command_available python3
export NUMMESSAGES=12
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
BLOCKED_FILE="$PWD/${RSYSLOG_DYNNAME}.missing/output.log"

write_seed_conf() {
	generate_conf
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.dequeueBatchSize="1" queue.saveOnShutdown="on")
:msg, contains, "msgnum:" action(type="omfile" file="'"$BLOCKED_FILE"'"
	createDirs="off" action.resumeRetryCount="-1")
'
}

write_da_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.size="50" queue.highWatermark="10" queue.lowWatermark="5"
	queue.dequeueBatchSize="1" queue.workerThreads="3"
	queue.workerThreadMinimumMessages="1" queue.checkpointInterval="1"
	queue.diskQueueType="auto" queue.diskQueueIdleTimeout="-1")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:00000000:" :omtesting:sleep 5 0
:msg, contains, "msgnum:00000001:" :omtesting:sleep 2 0
if ($msg contains "msgnum:") then
	action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

write_seed_conf
startup
injectmsg
state_deadline=$((SECONDS + 30))
while [ ! -s "$SPOOL_DIR/mainq.segq/state" ]; do
	[ "$SECONDS" -lt "$state_deadline" ] || error_exit 1 "seed segmented state was not created"
	./msleep 25
done
wait_rsyslog_instance_pid ""
. "$srcdir/diag.sh" kill-immediate
wait_shutdown

python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.frontier-baseline.json"
baseline_sequence=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(max(s["committed_sequence"] for s in d["state_slots"] if s["valid"]))' \
	"${RSYSLOG_DYNNAME}.frontier-baseline.json")
held_sequence=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(min(r["local_sequence"] for s in d["segments"] for r in s["records"] if r["message_number"] == 0))' \
	"${RSYSLOG_DYNNAME}.frontier-baseline.json")

write_da_conf
startup
wait_content '00000002' "$RSYSLOG_OUT_LOG"
check_not_present '00000000' "$RSYSLOG_OUT_LOG"
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.frontier-blocked.json"
blocked_sequence=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(max(s["committed_sequence"] for s in d["state_slots"] if s["valid"]))' \
	"${RSYSLOG_DYNNAME}.frontier-blocked.json")
if [ "$blocked_sequence" -ne $((held_sequence - 1)) ]; then
	printf 'FAIL: DA durable frontier crossed delayed first batch: sequence=%s held=%s baseline=%s\n' \
		"$blocked_sequence" "$held_sequence" "$baseline_sequence"
	error_exit 1
fi

wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.frontier-complete.json"
complete_sequence=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(max(s["committed_sequence"] for s in d["state_slots"] if s["valid"]))' \
	"${RSYSLOG_DYNNAME}.frontier-complete.json")
if [ "$complete_sequence" -lt $((held_sequence + NUMMESSAGES - 1)) ]; then
	printf 'FAIL: DA durable frontier did not cross completed suffix: sequence=%s held=%s\n' \
		"$complete_sequence" "$held_sequence"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
$RS_SORTCMD "$RSYSLOG_OUT_LOG" >"${RSYSLOG_DYNNAME}.sorted"
seq -f '%08g' 0 11 >"${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.sorted"
rm -rf "$SPOOL_DIR"
exit_test
