#!/bin/bash
# Test concurrency of message variables
# Added 2015-11-03 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
export TCPFLOOD_EXTRA_OPTS="-M'msg:msg: 1:2, 3:4, 5:6, 7:8 b test'"
echo ===============================================================================
echo \[msgvar-concurrency-array-event.tags.sh\]: testing concurrency of local variables
. $srcdir/diag.sh init
. $srcdir/diag.sh startup msgvar-concurrency-array-event.tags.conf
sleep 1
. $srcdir/diag.sh tcpflood -m500000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
#. $srcdir/diag.sh seq-check 0 499999
. $srcdir/diag.sh exit
