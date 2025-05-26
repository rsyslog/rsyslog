#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000 # this test is veeeeery slow, value is a compromise
generate_conf
add_conf '
$ModLoad ../plugins/omlibdbi/.libs/omlibdbi

$ActionQueueType LinkedList
$ActionQueueTimeoutEnqueue 15000

$ActionLibdbiDriver mysql
$ActionLibdbiHost 127.0.0.1
$ActionLibdbiUserName rsyslog
$ActionLibdbiPassword testbench
$ActionLibdbiDBName '$RSYSLOG_DYNNAME'
:msg, contains, "msgnum:" {
	:omlibdbi:
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.syncfile")
}
'
mysql_prep_for_test
startup
injectmsg
wait_file_lines $RSYSLOG_DYNNAME.syncfile $NUMMESSAGES 2500
shutdown_when_empty
wait_shutdown 
mysql_get_data
seq_check
exit_test
