#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Verify that omprog transaction commit failures do not spin in a tight retry
# loop. The helper fails the first two transaction commits and records commit
# timestamps with the transaction payload. The oracle checks retries of the
# same suspended payload, not the next independent payload, because the next
# message may be processed immediately after the suspended one succeeds. A
# one-second lower bound is used for a two-second interval so the test tolerates
# coarse scheduling while still catching immediate retry-loop resubmission.

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
	action(
		type="omprog"
		binary=`echo $srcdir/testsuites/omprog-resume-interval-bin.sh`
		template="outfmt"
		name="omprog_action"
		queue.type="LinkedList"
		queue.dequeueBatchSize="2"
		confirmMessages="on"
		useTransactions="on"
		action.resumeRetryCount="-1"
		action.resumeInterval="2"
	)
}
'
startup
injectmsg 0 2
shutdown_when_empty
wait_shutdown

mapfile -t commit_times < <(awk '/^commit / && /msgnum:00000000:/ { print $3 }' "$RSYSLOG_OUT_LOG")
if [[ ${#commit_times[@]} -lt 2 ]]; then
	echo "expected at least two commits for msgnum:00000000, got ${#commit_times[@]}"
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

retry_gap=$((commit_times[1] - commit_times[0]))
if [[ $retry_gap -lt 1 ]]; then
	echo "commit retry happened before resume interval pacing"
	echo "retry gap: $retry_gap"
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
