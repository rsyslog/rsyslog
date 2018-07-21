#!/bin/bash
# Test imptcp with many dropping connections
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_conndrop.sh\]: test imptcp with random connection drops
. $srcdir/diag.sh init
startup imptcp_conndrop.conf
# 100 byte messages to gain more practical data use
. $srcdir/diag.sh tcpflood -c20 -m50000 -r -d100 -P129 -D
sleep 10 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 49999 -E
exit_test
