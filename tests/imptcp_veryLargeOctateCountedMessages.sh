#!/bin/bash
# Test imptcp with poller not processing any messages
# added 2015-10-17 by singh.janmejay
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_veryLargeOctateCountedMessages.sh\]: test imptcp with very large messages while poller driven processing is disabled
. $srcdir/diag.sh init
startup_silent imptcp_nonProcessingPoller.conf
tcpflood -c1 -m20000 -r -d100000 -P129 -O
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty
wait_shutdown
seq_check 0 19999 -E -T
exit_test
