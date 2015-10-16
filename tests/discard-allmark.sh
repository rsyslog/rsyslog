#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[discard-allmark.sh\]: testing discard-allmark functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh startup discard-allmark.conf
. $srcdir/diag.sh tcpflood -m10 -i1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 2 10
. $srcdir/diag.sh exit
