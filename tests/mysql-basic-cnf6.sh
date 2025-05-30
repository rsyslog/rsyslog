#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
generate_conf
add_conf '
$ModLoad ../plugins/ommysql/.libs/ommysql
if $msg contains "msgnum" then {
	action(type="ommysql" server="127.0.0.1"
	       db="'$RSYSLOG_DYNNAME'" uid="rsyslog" pwd="testbench")
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
