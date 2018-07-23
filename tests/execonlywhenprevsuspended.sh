#!/bin/bash
# we test the execonly if previous is suspended directive. This is the
# most basic test which soley tests a singel case but no dependencies within
# the ruleset.
# rgerhards, 2010-06-23
echo =====================================================================================
echo \[execonlywhenprevsuspended.sh\]: test execonly...suspended functionality simple case
. $srcdir/diag.sh init
startup execonlywhenprevsuspended.conf
. $srcdir/diag.sh injectmsg 0 1000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 1 999
exit_test
