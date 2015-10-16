#!/bin/bash
# we test the execonly if previous is suspended directive.
# This test checks if, within the same rule, one action can be set
# to emit only if the previous was suspended while the next action
# always sends data.
# rgerhards, 2010-06-24
echo ===============================================================================
echo \[execonlywhenprevsuspended3.sh\]: test execonly...suspended functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh startup execonlywhenprevsuspended3.conf
. $srcdir/diag.sh injectmsg 0 1000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
echo check file 1
. $srcdir/diag.sh seq-check 1 999
echo check file 2
. $srcdir/diag.sh seq-check2 0 999
. $srcdir/diag.sh exit
