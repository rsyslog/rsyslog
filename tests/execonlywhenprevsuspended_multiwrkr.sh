# rgerhards, 2013-12-05
echo =====================================================================================
echo \[execonlywhenprevsuspended_multiwrkr.sh\]: test execonly...suspended functionality multiworker case
source $srcdir/diag.sh init
source $srcdir/diag.sh startup execonlywhenprevsuspended_multiwrkr.conf
source $srcdir/diag.sh injectmsg 0 1000
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 1 999
source $srcdir/diag.sh exit
