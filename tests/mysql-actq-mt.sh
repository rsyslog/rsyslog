#!/bin/bash
# Verify that ommysql preserves all messages when a bounded linked-list action
# queue is drained by multiple worker threads.  The queue size is intentionally
# small compared to the injected message count so the test covers backpressure.
# Shutdown is synchronized on the final MySQL row count and sequence, not just
# the main queue, because a slow CI database can still be draining the action
# queue after imdiag reports the main queue empty.
# The 80s enqueue timeout is a CI tolerance budget: if a stressed MySQL service
# cannot drain the action queue in that time, the failure should be investigated
# instead of silently turning the test into an unbounded wait.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=150000
mysql_actq_count_ready() {
	count=$(mysql -N -s --user=rsyslog --password=testbench \
		--database "$RSYSLOG_DYNNAME" \
		-e "select count(*) from SystemEvents;" \
		2> "$RSYSLOG_DYNNAME.mysqlerr")
	ret=$?
	grep -iv "Using a password on the command line interface can be insecure." \
		< "$RSYSLOG_DYNNAME.mysqlerr"
	if [ "$ret" -ne 0 ]; then
		error_exit "$ret"
	fi
	[ "$count" -ge "$NUMMESSAGES" ]
}
mysql_actq_data_ready() {
	mysql_actq_count_ready || return 1
	mysql_get_data
	seq_check --check-only
}
export QUEUE_EMPTY_CHECK_FUNC=mysql_actq_data_ready
generate_conf
add_conf '
module(load="../plugins/ommysql/.libs/ommysql")

:msg, contains, "msgnum:" {
	action(type="ommysql" server="127.0.0.1"
	db="'$RSYSLOG_DYNNAME'" uid="rsyslog" pwd="testbench"
	queue.size="10000" queue.type="linkedList"
	queue.workerthreads="5"
	queue.workerthreadMinimumMessages="500"
	queue.timeoutWorkerthreadShutdown="1000"
	queue.timeoutEnqueue="80000"
	queue.timeoutShutdown="30000"
	)
} 
'
mysql_prep_for_test
startup
injectmsg
shutdown_when_empty
wait_shutdown 
mysql_get_data
seq_check
mysql_cleanup_test
exit_test
