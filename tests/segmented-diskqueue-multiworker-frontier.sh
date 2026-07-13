#!/bin/bash
# Three one-record workers delay batch 1 while later batches finish. Later
# output proves workers continue taking work; state inspection must keep the
# durable frontier before batch 1, then advance across the completed suffix
# when batch 1 finishes. Output ordering itself is intentionally unspecified.
. ${srcdir:=.}/diag.sh init
check_command_available python3
export NUMMESSAGES=12
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

generate_conf
add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'$SPOOL_DIR'")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="16m" queue.dequeueBatchSize="1"
	queue.workerThreads="3" queue.workerThreadMinimumMessages="1"
	queue.checkpointInterval="1")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:00000000:" :omtesting:sleep 5 0
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
wait_queueempty
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.frontier-baseline.json"
baseline_sequence=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(max(s["committed_sequence"] for s in d["state_slots"] if s["valid"]))' \
	"${RSYSLOG_DYNNAME}.frontier-baseline.json")
injectmsg 0 1
$TESTTOOL_DIR/msleep 100
injectmsg 1 11
wait_content '00000002' "$RSYSLOG_OUT_LOG"
check_not_present '00000000' "$RSYSLOG_OUT_LOG"
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.frontier-blocked.json"
blocked_sequence=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(max(s["committed_sequence"] for s in d["state_slots"] if s["valid"]))' \
	"${RSYSLOG_DYNNAME}.frontier-blocked.json")
if [ "$blocked_sequence" -ne "$baseline_sequence" ]; then
	printf 'FAIL: durable frontier crossed delayed first batch: sequence=%s\n' "$blocked_sequence"
	error_exit 1
fi
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.frontier-complete.json"
complete_sequence=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(max(s["committed_sequence"] for s in d["state_slots"] if s["valid"]))' \
	"${RSYSLOG_DYNNAME}.frontier-complete.json")
if [ "$complete_sequence" -lt $((baseline_sequence + NUMMESSAGES)) ]; then
	printf 'FAIL: durable frontier did not advance over completed suffix: sequence=%s\n' "$complete_sequence"
	error_exit 1
fi
shutdown_when_empty
wait_shutdown
$RS_SORTCMD "$RSYSLOG_OUT_LOG" >"${RSYSLOG_DYNNAME}.sorted"
seq -f '%08g' 0 $((NUMMESSAGES - 1)) >"${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.sorted"
rm -rf "$SPOOL_DIR"
exit_test
