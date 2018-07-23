#!/bin/bash
# added 2015-09-29 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_re_match.sh\]: test re_match rscript-fn
. $srcdir/diag.sh init
startup rscript_re_match.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh content-check "*Matched*"
exit_test
