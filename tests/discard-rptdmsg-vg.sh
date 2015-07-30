#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[discard-rptdmsg.sh\]: testing discard-rptdmsg functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg discard-rptdmsg.conf
. $srcdir/diag.sh tcpflood -m10 -i1
# we need to give rsyslog a little time to settle the receiver
./msleep 1500
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 2 10
. $srcdir/diag.sh exit
