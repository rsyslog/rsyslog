#!/bin/bash
# Verify segmentedDisk recovers unprocessed queue data after an interrupted
# shutdown. The output action is blocked and rsyslogd is killed before shutdown
# can drain the full backlog; the oracle is final sequence completeness with
# duplicates allowed, because the limited-duplication model may replay
# uncommitted records.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=3000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"

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
injectmsg 0 "$NUMMESSAGES"
shutdown_immediate
. "$srcdir/diag.sh" kill-immediate
wait_shutdown
if [ -f "$RSYSLOG_OUT_LOG" ]; then
	pre_restart_lines=$(wc -l < "$RSYSLOG_OUT_LOG")
else
	pre_restart_lines=0
fi
if [ "$pre_restart_lines" -ge "$NUMMESSAGES" ]; then
	echo "FAIL: restart test did not leave a backlog; $pre_restart_lines messages were written before restart"
	error_exit 1
fi
if [ ! -f "$SPOOL_DIR/mainq.segq/meta" ]; then
	echo "FAIL: segmentedDisk metadata was not persisted before restart"
	error_exit 1
fi
if ! find "$SPOOL_DIR/mainq.segq" -maxdepth 1 -type f \( -name 'segment-*.seg' -o -name 'segment-*.open' \) \
	| grep -q .; then
	echo "FAIL: segmentedDisk segment files were not persisted before restart"
	error_exit 1
fi

write_conf '# no delay on restart'
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check_dupes
wait_seq_check_dupes() {
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
}
startup
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
