# rgerhards, 2015-05-27
echo =====================================================================================
echo \[execonlywhenprevsuspended-queue.sh\]: test execonly...suspended functionality with action on its own queue

source $srcdir/diag.sh init
source $srcdir/diag.sh startup execonlywhenprevsuspended-queue.conf
source $srcdir/diag.sh injectmsg 0 1000
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 1 999
source $srcdir/diag.sh exit
