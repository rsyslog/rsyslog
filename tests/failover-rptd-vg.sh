#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# Under Valgrind with repeat reduction enabled, suspend the TCP action by
# using an unavailable target. After shutdown, require a clean Valgrind
# result and all 5000 sequence numbers in the fallback file.

uname
if [ $(uname) = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[failover-rptd.sh\]: rptd test for failover functionality
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$RepeatedMsgReduction on

$template outfmt,"%msg:F,58:2%\n"
# note: the target server shall not be available!
:msg, contains, "msgnum:" @@127.0.0.1:13514
$ActionExecOnlyWhenPreviousIsSuspended on
& ./'"${RSYSLOG_OUT_LOG}"';outfmt
'
startup_vg
injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
check_exit_vg
seq_check  0 4999
exit_test
