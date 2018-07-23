#!/bin/bash
# Check if a set statement can correctly be reset to a different value
# Copyright 2014-11-24 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_set_modify.sh\]: testing set twice
. $srcdir/diag.sh init
startup rscript_set_modify.conf
. $srcdir/diag.sh injectmsg  0 100
shutdown_when_empty
wait_shutdown 
seq_check  0 99
exit_test
