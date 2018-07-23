#!/bin/bash
# we test the execonly if previous is suspended directive. For this,
# we have an action that is suspended for all messages but the second.
# we write two files: one only if the output is suspended and the other one
# in all cases. This should thouroughly check the logic involved.
# rgerhards, 2010-06-23
echo ===============================================================================
echo \[execonlywhenprevsuspended2.sh\]: test execonly...suspended functionality
. $srcdir/diag.sh init
startup execonlywhenprevsuspended2.conf
. $srcdir/diag.sh injectmsg 0 1000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
echo check file 1
seq_check 1 999
echo check file 2
seq_check2 0 999
exit_test
