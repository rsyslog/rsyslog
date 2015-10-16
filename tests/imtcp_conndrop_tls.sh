#!/bin/bash
# Test imtcp/TLS with many dropping connections
# added 2011-06-09 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imtcp_conndrop_tls.sh\]: test imtcp/tls with random connection drops
cat rsyslog.action.1.include
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imtcp_conndrop.conf
# 100 byte messages to gain more practical data use
. $srcdir/diag.sh tcpflood -c20 -m50000 -r -d100 -P129 -D
sleep 10 # due to large messages, we need this time for the tcp receiver to settle...
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
. $srcdir/diag.sh seq-check 0 49999 -E
. $srcdir/diag.sh exit
