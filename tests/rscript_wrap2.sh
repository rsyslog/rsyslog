#!/bin/bash
# added 2014-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_wrap2.sh\]: a test for wrap\(2\) script-function
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rscript_wrap2.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh content-check "**foo says at Thu Oct 30 13:20:18 IST 2014 random number is 19597**"
. $srcdir/diag.sh exit
