# added 2014-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_wrap2.sh\]: a test for wrap\(2\) script-function
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_wrap2.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check "**foo says at Thu Oct 30 13:20:18 IST 2014 random number is 19597**"
source $srcdir/diag.sh exit
