#!/bin/bash
# test many concurrent tcp connections
echo ====================================================================================
echo TEST: \[manyptcp.sh\]: test imptcp with large connection count
. $srcdir/diag.sh init
. $srcdir/diag.sh startup manyptcp.conf
# the config file specifies exactly 1100 connections
. $srcdir/diag.sh tcpflood -c1000 -m40000
# the sleep below is needed to prevent too-early termination of the tcp listener
sleep 1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh seq-check 0 39999
. $srcdir/diag.sh exit
