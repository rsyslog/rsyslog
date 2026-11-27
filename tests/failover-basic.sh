#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# An unavailable TCP target suspends the primary action. The conditional
# file action must record the complete sequence 0 through 4999 after
# shutdown.
echo ===============================================================================
echo \[failover-basic.sh\]: basic test for failover functionality
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$template outfmt,"%msg:F,58:2%\n"
# note: the target server shall not be available!
:msg, contains, "msgnum:" @@127.0.0.1:13514
$ActionExecOnlyWhenPreviousIsSuspended on
& ./'"${RSYSLOG_OUT_LOG}"';outfmt
'
startup
injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
seq_check  0 4999
exit_test
