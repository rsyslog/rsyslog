#!/bin/bash
# check if execonly...suspended works when the first action is *not*
# suspended --> file1 must be created, file 2 not
# rgerhards, 2015-05-27
echo =====================================================================================
echo \[execonlywhenprevsuspended-nonsusp-queue\]: test execonly...suspended functionality with non-suspended action and queue

. $srcdir/diag.sh init
. $srcdir/diag.sh startup execonlywhenprevsuspended-nonsusp-queue.conf
. $srcdir/diag.sh injectmsg 0 1000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
ls *.out.log
. $srcdir/diag.sh seq-check 0 999
if [ -e rsyslog2.out.log ]; then
    echo "error: \"suspended\" file exists, first 10 lines:"
    head rsyslog2.out.log
    exit 1
fi
. $srcdir/diag.sh exit
