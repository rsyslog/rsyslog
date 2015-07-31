#!/bin/bash
# Test imptcp with large messages
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_large.sh\]: test imptcp with large-size messages
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imptcp_large.conf
# send 4000 messages of 10.000bytes plus header max, randomized
. $srcdir/diag.sh tcpflood -c5 -m20000 -r -d10000 -P129
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
. $srcdir/diag.sh seq-check 0 19999 -E
. $srcdir/diag.sh exit
