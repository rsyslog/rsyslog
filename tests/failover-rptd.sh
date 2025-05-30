#!/bin/bash
# rptd test for failover functionality
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
NUMMESSAGES=5000
generate_conf
add_conf '
$RepeatedMsgReduction on

$template outfmt,"%msg:F,58:2%\n"
# note: the target server shall not be available!
:msg, contains, "msgnum:" @@127.0.0.1:'$TCPFLOOD_PORT'
$ActionExecOnlyWhenPreviousIsSuspended on
& ./'$RSYSLOG_OUT_LOG';outfmt
'
startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown 
seq_check
exit_test
