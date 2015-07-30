#!/bin/bash
# Test for "stop" statement
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[stop.sh\]: testing stop statement
. $srcdir/diag.sh init
. $srcdir/diag.sh startup stop.conf
sleep 1
. $srcdir/diag.sh tcpflood -m10 -i1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 2 10
. $srcdir/diag.sh exit
