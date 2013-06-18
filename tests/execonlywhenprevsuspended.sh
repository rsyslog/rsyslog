# we test the execonly if previous is suspended directive. This is the
# most basic test which soley tests a singel case but no dependencies within
# the ruleset.
# rgerhards, 2010-06-23
echo =====================================================================================
echo \[execonlywhenprevsuspended.sh\]: test execonly...suspended functionality simple case
source $srcdir/diag.sh init
source $srcdir/diag.sh startup execonlywhenprevsuspended.conf
source $srcdir/diag.sh injectmsg 0 1000
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 1 999
source $srcdir/diag.sh exit
