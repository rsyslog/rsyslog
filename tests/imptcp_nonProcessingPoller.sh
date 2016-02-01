# Test imptcp with poller not processing any messages
# added 2015-10-16 by singh.janmejay
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_nonProcessingPoller.sh\]: test imptcp with poller driven processing disabled
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imptcp_nonProcessingPoller.conf
. $srcdir/diag.sh tcpflood -c1 -m20000 -r -d10000 -P129 -O
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 19999 -E
. $srcdir/diag.sh exit
