# added 2014-10-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_variable.sh\]: basic test for mmnormalize module variable-support
source $srcdir/diag.sh init
source $srcdir/diag.sh startup mmnormalize_variable.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check  "h:13 m:20 s:18"
source $srcdir/diag.sh exit
