#!/bin/bash
# Test imptcp with large messages
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_large.sh\]: test imptcp with large-size messages
. $srcdir/diag.sh init
startup imptcp_large.conf
# send 4000 messages of 10.000bytes plus header max, randomized
. $srcdir/diag.sh tcpflood -c5 -m20000 -r -d10000 -P129
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 19999 -E
exit_test
