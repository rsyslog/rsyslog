#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# basic test for mysql functionality
. ${srcdir:=.}/diag.sh init
	# DEBUGGING - REMOVE ME #
	ls -l mysql*log
	sudo cat /var/log/mysql/error.log ## TODO: remove me
	df -h
export NUMMESSAGES=5000
generate_conf
add_conf '
$ModLoad ../plugins/ommysql/.libs/ommysql
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
