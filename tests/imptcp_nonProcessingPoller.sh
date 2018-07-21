#!/bin/bash
# Test imptcp with poller not processing any messages
# added 2015-10-16 by singh.janmejay
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_nonProcessingPoller.sh\]: test imptcp with poller driven processing disabled
. $srcdir/diag.sh init
startup imptcp_nonProcessingPoller.conf
. $srcdir/diag.sh tcpflood -c1 -m20000 -r -d10000 -P129 -O
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty
wait_shutdown
seq_check 0 19999 -E
exit_test
