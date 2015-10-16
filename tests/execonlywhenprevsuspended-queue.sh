#!/bin/bash
# rgerhards, 2015-05-27
echo =====================================================================================
echo \[execonlywhenprevsuspended-queue.sh\]: test execonly...suspended functionality with action on its own queue

. $srcdir/diag.sh init
. $srcdir/diag.sh startup execonlywhenprevsuspended-queue.conf
. $srcdir/diag.sh injectmsg 0 1000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 1 999
. $srcdir/diag.sh exit
