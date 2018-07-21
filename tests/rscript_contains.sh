#!/bin/bash
# added 2012-09-14 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_contains.sh\]: test for contains script-filter
. $srcdir/diag.sh init
startup rscript_contains.conf
. $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
seq_check  0 4999
exit_test
