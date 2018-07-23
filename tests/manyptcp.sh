#!/bin/bash
# test many concurrent tcp connections
echo ====================================================================================
echo TEST: \[manyptcp.sh\]: test imptcp with large connection count
. $srcdir/diag.sh init
startup manyptcp.conf
# the config file specifies exactly 1100 connections
. $srcdir/diag.sh tcpflood -c1000 -m40000
# the sleep below is needed to prevent too-early termination of the tcp listener
sleep 1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 39999
exit_test
