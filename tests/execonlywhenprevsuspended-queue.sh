#!/bin/bash
# rgerhards, 2015-05-27
echo =====================================================================================
echo \[execonlywhenprevsuspended-queue.sh\]: test execonly...suspended functionality with action on its own queue

uname
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init
startup execonlywhenprevsuspended-queue.conf
. $srcdir/diag.sh injectmsg 0 1000
shutdown_when_empty
wait_shutdown
seq_check 1 999
exit_test
