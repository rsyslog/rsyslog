#!/bin/bash
# we test the execonly if previous is suspended directive.
# This test checks if, within the same rule, one action can be set
# to emit only if the previous was suspended while the next action
# always sends data.
# rgerhards, 2010-06-24
echo ===============================================================================
echo \[execonlywhenprevsuspended3.sh\]: test execonly...suspended functionality
. $srcdir/diag.sh init
startup execonlywhenprevsuspended3.conf
. $srcdir/diag.sh injectmsg 0 1000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
echo check file 1
seq_check 1 999
echo check file 2
seq_check2 0 999
exit_test
