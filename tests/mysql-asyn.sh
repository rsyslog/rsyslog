#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# asyn test for mysql functionality (running on async action queue)
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
generate_conf
add_conf '
$ModLoad ../plugins/ommysql/.libs/ommysql
$ActionQueueType LinkedList
$ActionQueueTimeoutEnqueue 20000
:msg, contains, "msgnum:" :ommysql:127.0.0.1,'$RSYSLOG_DYNNAME',rsyslog,testbench;
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
