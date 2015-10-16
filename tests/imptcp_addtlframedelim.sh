#!/bin/bash
# added 2010-08-11 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_addtlframedelim.sh\]: test imptcp additional frame delimiter
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imptcp_addtlframedelim.conf
. $srcdir/diag.sh tcpflood -m20000 -F0 -P129
#sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
. $srcdir/diag.sh seq-check 0 19999
. $srcdir/diag.sh exit
