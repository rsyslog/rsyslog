#!/bin/bash
# Check if a set statement to the same subtree does not reset
# other variables in that same subtree.
# Copyright 2014-11-24 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_unaffected_reset.sh\]: testing set/reset
. $srcdir/diag.sh init
startup rscript_unaffected_reset.conf
. $srcdir/diag.sh injectmsg  0 100
shutdown_when_empty
wait_shutdown 
seq_check  0 99
exit_test
