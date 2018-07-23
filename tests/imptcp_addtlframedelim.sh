#!/bin/bash
# added 2010-08-11 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_addtlframedelim.sh\]: test imptcp additional frame delimiter
. $srcdir/diag.sh init
startup imptcp_addtlframedelim.conf
. $srcdir/diag.sh tcpflood -m20000 -F0 -P129
#sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 19999
exit_test
