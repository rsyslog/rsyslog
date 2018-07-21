#!/bin/bash
# Test for fixedArray queue mode
# added 2009-05-20 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. $srcdir/diag.sh init
startup arrayqueue.conf

# 40000 messages should be enough
. $srcdir/diag.sh injectmsg  0 40000
shutdown_when_empty
wait_shutdown 
seq_check 0 39999
exit_test
