#!/bin/bash
# Test concurrency of exec_template function with msg variables
# Added 2015-12-16 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[prop-all-json-concurrency.sh\]: testing concurrency of $!all-json variables
. $srcdir/diag.sh init
startup prop-all-json-concurrency.conf
sleep 1
. $srcdir/diag.sh tcpflood -m500000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 499999
exit_test
