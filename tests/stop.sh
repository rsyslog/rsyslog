#!/bin/bash
# Test for "stop" statement
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[stop.sh\]: testing stop statement
. $srcdir/diag.sh init
startup stop.conf
sleep 1
. $srcdir/diag.sh tcpflood -m10 -i1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 2 10
exit_test
