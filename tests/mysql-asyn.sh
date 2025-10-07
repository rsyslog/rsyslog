#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# asyn test for mysql functionality (running on async action queue)

. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=25000
generate_conf
add_conf '
$ModLoad ../plugins/ommysql/.libs/ommysql
$ActionQueueType LinkedList
$ActionQueueTimeoutEnqueue 1000
if $msg contains "msgnum:" then {
  action(
    type="ommysql"
    server="127.0.0.1"
    db="'$RSYSLOG_DYNNAME'"
    uid="rsyslog"
    pwd="testbench"
    queue.type="LinkedList"
    queue.timeoutEnqueue="10000"
    queue.workerThreads="2"
    queue.workerThreadMinimumMessages="64"
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
