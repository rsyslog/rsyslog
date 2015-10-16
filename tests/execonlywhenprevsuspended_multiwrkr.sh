#!/bin/bash
# rgerhards, 2013-12-05
echo =====================================================================================
echo \[execonlywhenprevsuspended_multiwrkr.sh\]: test execonly...suspended functionality multiworker case
. $srcdir/diag.sh init
. $srcdir/diag.sh startup execonlywhenprevsuspended_multiwrkr.conf
. $srcdir/diag.sh injectmsg 0 1000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 1 999
. $srcdir/diag.sh exit
