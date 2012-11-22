# Test imptcp with large messages
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_large.sh\]: test imptcp with large-size messages
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imptcp_large.conf
# send 4000 messages of 10.000bytes plus header max, randomized
source $srcdir/diag.sh tcpflood -c5 -m20000 -r -d10000 -P129
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 19999 -E
source $srcdir/diag.sh exit
