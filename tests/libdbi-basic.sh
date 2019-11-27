#!/bin/bash
# basic test for libdbi-basic functionality via mysql
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
generate_conf
add_conf '
$ModLoad ../plugins/omlibdbi/.libs/omlibdbi
$ActionLibdbiDriver mysql
$ActionLibdbiHost 127.0.0.1
$ActionLibdbiUserName rsyslog
$ActionLibdbiPassword testbench
$ActionLibdbiDBName '$RSYSLOG_DYNNAME'
:msg, contains, "msgnum:" :omlibdbi:
'
mysql_prep_for_test
startup
injectmsg
shutdown_when_empty
wait_shutdown 
mysql_get_data
seq_check
exit_test
