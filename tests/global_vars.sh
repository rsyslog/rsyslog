#!/bin/bash
# Test for global variables
# added 2013-11-18 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[global_vars.sh\]: testing global variable support
. $srcdir/diag.sh init
startup global_vars.conf

# 40000 messages should be enough
. $srcdir/diag.sh injectmsg  0 40000

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown 
seq_check 0 39999
exit_test
