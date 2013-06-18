# Test imptcp with many dropping connections
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_conndrop.sh\]: test imptcp with random connection drops
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imptcp_conndrop.conf
# 100 byte messages to gain more practical data use
source $srcdir/diag.sh tcpflood -c20 -m50000 -r -d100 -P129 -D
sleep 4 # due to large messages, we need this time for the tcp receiver to settle...
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 49999 -E
source $srcdir/diag.sh exit
