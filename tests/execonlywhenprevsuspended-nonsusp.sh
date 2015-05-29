# check if execonly...suspended works when the first action is *not*
# suspended --> file1 must be created, file 2 not
# rgerhards, 2015-05-27
echo =====================================================================================
echo \[execonlywhenprevsuspended-nonsusp\]: test execonly...suspended functionality with non-suspended action

source $srcdir/diag.sh init
source $srcdir/diag.sh startup execonlywhenprevsuspended-nonsusp.conf
source $srcdir/diag.sh injectmsg 0 1000
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 999
if [ -e rsyslog2.out.log ]; then
    echo "error: \"suspended\" file exists, first 10 lines:"
    head rsyslog2.out.log
    exit 1
fi
source $srcdir/diag.sh exit
