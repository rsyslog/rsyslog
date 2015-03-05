# added 2014-10-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_replace.sh\]: test for replace script-function
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_replace.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check  "date time: Thu 0ct0ber 30 13:20:18 IST 2014"
source $srcdir/diag.sh exit
