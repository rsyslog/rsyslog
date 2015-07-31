#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2013-11-22
echo ===============================================================================
echo \[rfc5424parser.sh\]: testing mmpstrucdata
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rfc5424parser.conf
sleep 1
. $srcdir/diag.sh tcpflood -m100 -y
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99
. $srcdir/diag.sh exit
