#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
echo ====================================================================================
echo TEST: \[imptcp_spframingfix.sh\]: test imptcp in regard to Cisco ASA framing fix
. $srcdir/diag.sh init
startup imptcp_spframingfix.conf
. $srcdir/diag.sh tcpflood -B -I ${srcdir}/testsuites/spframingfix.testdata
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 19
exit_test
