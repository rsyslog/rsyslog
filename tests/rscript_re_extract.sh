#!/bin/bash
# added 2015-09-29 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_re_extract.sh\]: test re_extract rscript-fn
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rscript_re_extract.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh content-check "*Number is 19597*"
. $srcdir/diag.sh exit
