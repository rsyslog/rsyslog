# Test imptcp with poller not processing any messages
# added 2015-10-17 by singh.janmejay
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_veryLargeOctateCountedMessages.sh\]: test imptcp with very large messages while poller driven processing is disabled
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-silent imptcp_nonProcessingPoller.conf
. $srcdir/diag.sh tcpflood -c1 -m20000 -r -d100000 -P129 -O
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 19999 -E -T
. $srcdir/diag.sh exit
