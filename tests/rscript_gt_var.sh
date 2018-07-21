#!/bin/bash
# added 2014-01-17 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_gt.sh\]: testing rainerscript GT statement for two JSON variables
. $srcdir/diag.sh init
startup rscript_gt_var.conf
. $srcdir/diag.sh injectmsg  0 1
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
seq_check  0 0
exit_test
