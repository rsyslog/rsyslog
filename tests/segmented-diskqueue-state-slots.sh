#!/bin/bash
# Verify that a torn newest state slot falls back to the previous valid slot.
# Phase one leaves a multi-segment backlog and corrupts only the newest slot;
# complete sequence recovery proves bounded reserved-ID reconciliation avoids
# both segment-ID reuse and message loss. Duplicates are allowed by checkpoint
# replay semantics.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
check_command_available python3
export NUMMESSAGES=1200
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
	queue.type="segmentedDisk"
	queue.filename="mainq"
	queue.maxFileSize="8k"
	queue.dequeueBatchSize="32"
	queue.checkpointInterval="64"
	queue.saveOnShutdown="on"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

write_conf ':omtesting:sleep 10 0'
startup
wait_rsyslog_instance_pid
injectmsg 0 "$NUMMESSAGES"
. "$srcdir/diag.sh" kill-immediate
wait_shutdown

python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" \
	--corrupt-newest-state-slot >"${RSYSLOG_DYNNAME}.state-slots.json"

write_conf '# no delay during fallback replay'
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check_dupes
wait_seq_check_dupes() {
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
}
startup
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
rm -rf "$SPOOL_DIR"
exit_test
