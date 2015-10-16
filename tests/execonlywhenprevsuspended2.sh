#!/bin/bash
# we test the execonly if previous is suspended directive. For this,
# we have an action that is suspended for all messages but the second.
# we write two files: one only if the output is suspended and the other one
# in all cases. This should thouroughly check the logic involved.
# rgerhards, 2010-06-23
echo ===============================================================================
echo \[execonlywhenprevsuspended2.sh\]: test execonly...suspended functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh startup execonlywhenprevsuspended2.conf
. $srcdir/diag.sh injectmsg 0 1000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
echo check file 1
. $srcdir/diag.sh seq-check 1 999
echo check file 2
. $srcdir/diag.sh seq-check2 0 999
. $srcdir/diag.sh exit
