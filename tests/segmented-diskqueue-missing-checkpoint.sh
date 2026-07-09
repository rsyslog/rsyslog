#!/bin/bash
# Verify a missing checkpoint causes replay of every valid record. A short
# phase-one delay allows a durable frontier to form while leaving a backlog;
# duplicate output is allowed because removing the checkpoint forgets it.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
	queue.type="segmentedDisk"
	queue.filename="mainq"
	queue.maxFileSize="32k"
	queue.dequeueBatchSize="32"
	queue.saveOnShutdown="on"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

write_conf ':omtesting:sleep 0 10000'
startup
injectmsg 0 "$NUMMESSAGES"
shutdown_immediate
. "$srcdir/diag.sh" kill-immediate
wait_shutdown
if [ ! -f "$SPOOL_DIR/mainq.segq/checkpoint" ]; then
	echo "FAIL: phase one did not create a checkpoint"
	error_exit 1
fi
rm "$SPOOL_DIR/mainq.segq/checkpoint"

write_conf '# no delay during replay'
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check_dupes
wait_seq_check_dupes() {
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
}
startup
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
if ! grep -q "checkpoint missing or invalid; replaying all valid records" "${RSYSLOG_DYNNAME}.started"; then
	echo "FAIL: missing-checkpoint replay was not reported"
	error_exit 1
fi
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
