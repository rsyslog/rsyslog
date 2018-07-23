#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imtcp_no_octet_counted.sh\]: test imtcp with octet counted framing disabled
. $srcdir/diag.sh init
startup imtcp_no_octet_counted.conf
. $srcdir/diag.sh tcpflood -B -I ${srcdir}/testsuites/no_octet_counted.testdata
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 19
exit_test
