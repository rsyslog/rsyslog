#!/bin/bash
# Verify targeted WTP wakeups across repeated queue-idle lifecycles. The first
# phase proves that a main-queue worker enters wtiWaitNonEmpty and the
# minimum-dequeue-batch wait times out. Worker scheduling intentionally does
# not decide whether w0 or w1 dequeues that partial batch. A separate ordered
# debug-log oracle also proves that w1 idles, times out, and completes
# its worker-exit cleanup before a later filtered enqueue restarts w1. This
# avoids a fixed sleep while preserving exact two-ID delivery as the data
# oracle.
. ${srcdir:=.}/diag.sh init
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.wtp-debug.log"

count_w1_starts() {
	if test -f "$RSYSLOG_DEBUGLOG"; then
		grep -c 'main Q:Reg/w1.*worker starting' "$RSYSLOG_DEBUGLOG" || true
	else
		echo 0
	fi
}

first_worker_remains_live() {
	if test ! -f "$RSYSLOG_DEBUGLOG"; then
		echo 0
		return
	fi
	awk '
		/main Q:Reg\/w0.*:.*worker starting/ { ++starts }
		/main Q:Reg\/w0.*Worker thread [0-9a-f]+, terminated/ { exited = 1 }
		END { print starts == 1 && !exited ? 1 : 0 }
	' "$RSYSLOG_DEBUGLOG"
}

# queue.c emits the first two events around wtiWaitNonEmpty(). wtp.c emits the
# last event only after w1's state has become WAIT_JOIN and its active-worker
# count has been decremented. The w1 lifecycle is ordered, while the generic
# min-batch events may occur on either side of it.
completed_w1_lifecycle() {
	if test ! -f "$RSYSLOG_DEBUGLOG"; then
		echo 0
		return
	fi
	awk '
		/waiting on queue to become non-empty/ { saw_wait = 1 }
		/minDeqBatchSize timeout/ { saw_minbatch_timeout = 1 }
		/main Q:Reg\/w1: worker IDLE, waiting for work/ { saw_w1_idle = 1 }
		saw_w1_idle && /main Q:Reg\/w1: inactivity timeout, worker terminating/ { saw_w1_timeout = 1 }
		saw_w1_timeout && /main Q:Reg\/w1.*Worker thread [0-9a-f]+, terminated/ { saw_w1_exit = 1 }
		END { print saw_wait && saw_minbatch_timeout && saw_w1_exit ? 1 : 0 }
	' "$RSYSLOG_DEBUGLOG"
}

generate_conf
add_conf '
# Keep worker-termination status messages from creating an unrelated main-queue
# enqueue; only the explicit filtered trigger may restart the stopped w1 slot.
global(processInternalMessages="off")
main_queue(queue.type="LinkedList" queue.workerThreads="2"
	queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.minDequeueBatchSize="2" queue.minDequeueBatchSize.timeout="200"
	queue.timeoutWorkerthreadShutdown="1000")
template(name="outfmt" type="string" string="id=%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'"$RSYSLOG_OUT_LOG"'")
'
startup

injectmsg 0 2
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 2
wait_file_lines --count-function completed_w1_lifecycle "$RSYSLOG_DEBUGLOG" 1
# For regular classic queues w0 is always-running: w1 may time out, but the
# final consumer remains available to handle the next burst without any
# exit/re-entry handoff.
wait_file_lines --count-function first_worker_remains_live "$RSYSLOG_DEBUGLOG" 1

# This reaches the same queue but is filtered from the output action. Its
# lifecycle effect is to require the already-terminated nonzero worker slot to
# restart. The preceding ordered predicate makes this start count a post-exit
# baseline rather than an observation of a still-running worker.
w1_starts_before_restart=$(count_w1_starts)
injectmsg_literal 'wtp targeted restart trigger'
wait_file_lines --count-function count_w1_starts "$RSYSLOG_DEBUGLOG" $((w1_starts_before_restart + 1))
wait_file_lines --count-function first_worker_remains_live "$RSYSLOG_DEBUGLOG" 1

shutdown_when_empty
wait_shutdown
for message in $(seq 0 1); do
	printf -v formatted_message '%08d' "$message"
	content_count_check --regex "^id=$formatted_message$" 1 "$RSYSLOG_OUT_LOG"
done
exit_test
