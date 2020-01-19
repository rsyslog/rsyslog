#!/bin/bash
# test for mysql with multithread actionq
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=150000
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
	queue.timeoutEnqueue="20000"
	)
} 
'
mysql_prep_for_test
startup
injectmsg 0 50000
wait_queueempty 
echo waiting for worker threads to timeout
./msleep 3000
injectmsg 50000 50000
wait_queueempty 
echo waiting for worker threads to timeout
./msleep 2000
injectmsg 100000 50000
shutdown_when_empty
wait_shutdown 
mysql_get_data
seq_check
mysql_cleanup_test
exit_test
