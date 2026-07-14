#!/bin/bash
# Two consecutive dirty restarts create two recovery tails. The final start
# must process them in segment-ID order before the new active segment; ordered
# sequence validation permits checkpoint duplicates but no gaps or reordering.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=800
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'$SPOOL_DIR'")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="1m" queue.dequeueBatchSize="16"
	queue.checkpointInterval="64" queue.saveOnShutdown="on")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

dirty_phase() {
	write_conf ':omtesting:sleep 10 0'
	startup
	wait_rsyslog_instance_pid
	injectmsg "$1" 400
	. "$srcdir/diag.sh" kill-immediate
	wait_shutdown
}

dirty_phase 0
dirty_phase 400
write_conf ':omtesting:sleep 10 0'
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check_dupes
wait_seq_check_dupes() {
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
}
startup
wait_rsyslog_instance_pid
recovery_count=$(find "$SPOOL_DIR/mainq.segq" -maxdepth 1 -name 'segment-*.recover' -type f | wc -l)
if [ "$recovery_count" -lt 2 ]; then
	printf 'FAIL: expected at least two recovery segments after repeated crashes, found %s\n' "$recovery_count"
	find "$SPOOL_DIR/mainq.segq" -maxdepth 1 -type f -print
	error_exit 1
fi
. "$srcdir/diag.sh" kill-immediate
wait_shutdown
write_conf '# final recovery has no delay'
startup
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
rm -rf "$SPOOL_DIR"
exit_test
