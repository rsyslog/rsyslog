#!/bin/bash
# Test concurrency of message variables
# Added 2015-11-03 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
export TCPFLOOD_EXTRA_OPTS="-M'msg:msg: 1:2, 3:4, 5:6, 7:8 b test'"
echo ===============================================================================
echo \[msgvar-concurrency-array.sh\]: testing concurrency of local variables
. $srcdir/diag.sh init
startup msgvar-concurrency-array.conf
sleep 1
. $srcdir/diag.sh tcpflood -m500000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
#seq_check 0 499999
exit_test
