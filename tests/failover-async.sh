#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# With a linked-list action queue and an unavailable TCP target, verify
# that the file action writes the entire sequence 0 through 4999 after
# shutdown.
echo ===============================================================================
echo \[failover-async.sh\]: async test for failover functionality

uname
if [ $(uname) = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$template outfmt,"%msg:F,58:2%\n"
# note: the target server shall not be available!

$ActionQueueType LinkedList
:msg, contains, "msgnum:" @@127.0.0.1:13514
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
