#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# rptd test for failover functionality - no failover
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# With repeat reduction enabled, send messages to a successful /dev/null
# action under Valgrind. Require a clean Valgrind result and an absent or
# empty fallback file after shutdown.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$RepeatedMsgReduction on

# second action should never execute
:msg, contains, "msgnum:" /dev/null
$ActionExecOnlyWhenPreviousIsSuspended on
& ./'"${RSYSLOG_OUT_LOG}"'
'
startup_vg
injectmsg  0 5000
shutdown_when_empty
wait_shutdown_vg
check_exit_vg
# now we need our custom logic to see if the result file is empty
# (what it should be!)
if [ -f "$RSYSLOG_OUT_LOG" ] && [ "$(cat "$RSYSLOG_OUT_LOG")" != "" ]; then
	echo "ERROR, output file not empty"
	cat -n "$RSYSLOG_OUT_LOG"
	error_exit 1
fi
exit_test
