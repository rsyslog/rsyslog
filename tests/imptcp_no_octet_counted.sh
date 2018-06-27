#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_no_octet_counted.sh\]: test imptcp with octet counted framing disabled
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imptcp_no_octet_counted.conf
. $srcdir/diag.sh tcpflood -B -I ${srcdir}/testsuites/no_octet_counted.testdata
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
. $srcdir/diag.sh seq-check 0 19
. $srcdir/diag.sh exit
