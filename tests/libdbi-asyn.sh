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
$ActionLibdbiDBName Syslog
:msg, contains, "msgnum:" {
	:omlibdbi:
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.syncfile")
}
'
mysql --user=rsyslog --password=testbench < ${srcdir}/testsuites/mysql-truncate.sql
startup
injectmsg  0 $NUMMESSAGES
wait_file_lines $RSYSLOG_DYNNAME.syncfile $NUMMESSAGES 2500
shutdown_when_empty
wait_shutdown 
# note "-s" is requried to suppress the select "field header"
mysql -s --user=rsyslog --password=testbench < ${srcdir}/testsuites/mysql-select-msg.sql > $RSYSLOG_OUT_LOG
seq_check  0 $(( NUMMESSAGES - 1 ))
exit_test
