#!/bin/bash
# Test for "stop" statement
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[stop-msgvar.sh\]: testing stop statement together with message variables
. $srcdir/diag.sh init
startup stop-msgvar.conf
sleep 1
. $srcdir/diag.sh tcpflood -m2000 -i1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 100 999
exit_test
