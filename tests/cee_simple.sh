#!/bin/bash
# added 2012-09-19 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[cee_simple.sh\]: basic CEE property test
. $srcdir/diag.sh init
startup cee_simple.conf
. $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
seq_check  0 4999
exit_test
