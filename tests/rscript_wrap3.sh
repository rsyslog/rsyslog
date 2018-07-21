#!/bin/bash
# added 2014-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_wrap3.sh\]: a test for wrap\(3\) script-function
. $srcdir/diag.sh init
startup rscript_wrap3.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/wrap3_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
. $srcdir/diag.sh content-check "bcdefbcfoo says a abcESCdefb has ESCbcdefbc"
exit_test
