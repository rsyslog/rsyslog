#!/bin/bash
# added 2014-10-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_variable.sh\]: basic test for mmnormalize module variable-support
. $srcdir/diag.sh init
. $srcdir/diag.sh startup mmnormalize_variable.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh content-check  "h:13 m:20 s:18"
. $srcdir/diag.sh exit
