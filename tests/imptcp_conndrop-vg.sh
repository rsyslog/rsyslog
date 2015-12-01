#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# Copyright (C) 2014 Rainer Gerhards -- 2014-11-14
echo ====================================================================================
echo TEST: \[imptcp_conndrop-vg.sh\]: test imptcp with random connection drops
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg imptcp_conndrop.conf
# 100 byte messages to gain more practical data use
. $srcdir/diag.sh tcpflood -c20 -m50000 -r -d100 -P129 -D
sleep 10 # due to large messages, we need this time for the tcp receiver to settle...
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown-vg    # and wait for it to terminate
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 0 49999 -E
. $srcdir/diag.sh exit
