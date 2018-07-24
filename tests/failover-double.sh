#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[failover-double.sh\]: test for double failover functionality
. $srcdir/diag.sh init
generate_conf
add_conf '
$template outfmt,"%msg:F,58:2%\n"

:msg, contains, "msgnum:" @@127.0.0.1:13516
$ActionExecOnlyWhenPreviousIsSuspended on
&	@@127.0.0.1:1234
&	./rsyslog.out.log;outfmt
$ActionExecOnlyWhenPreviousIsSuspended off
'
startup
. $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
seq_check  0 4999
exit_test
