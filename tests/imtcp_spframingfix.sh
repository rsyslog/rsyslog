#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
echo ====================================================================================
echo TEST: \[imptcp_spframingfix.sh\]: test imptcp in regard to Cisco ASA framing fix
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imtcp_spframingfix.conf
. $srcdir/diag.sh tcpflood -B -I ${srcdir}/testsuites/spframingfix.testdata
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
. $srcdir/diag.sh seq-check 0 19
. $srcdir/diag.sh exit
