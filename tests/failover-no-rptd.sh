#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# With repeat reduction enabled, a successful primary /dev/null action
# must leave the conditional fallback file empty after shutdown.
echo ===============================================================================
echo \[failover-no-rptd.sh\]: rptd test for failover functionality - no failover
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$RepeatedMsgReduction on

# second action should never execute
:msg, contains, "msgnum:" /dev/null
$ActionExecOnlyWhenPreviousIsSuspended on
& ./'"${RSYSLOG_OUT_LOG}"'
'
startup
injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
# now we need our custom logic to see if the result file is empty
# (what it should be!)
cmp $RSYSLOG_OUT_LOG /dev/null
if [ $? -eq 1 ]
then
	echo "ERROR, output file not empty"
	exit 1
fi
exit_test
