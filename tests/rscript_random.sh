#!/bin/bash
# added 2015-06-22 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_random.sh\]: test for random-number-generator script-function
. $srcdir/diag.sh init
startup rscript_random.conf
. $srcdir/diag.sh tcpflood -m 20
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
. $srcdir/diag.sh content-pattern-check "^[0-9]$"
exit_test
