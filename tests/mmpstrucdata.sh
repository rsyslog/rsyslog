#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2013-11-22
echo ===============================================================================
echo \[mmpstrucdata.sh\]: testing mmpstrucdata
. $srcdir/diag.sh init
startup mmpstrucdata.conf
sleep 1
. $srcdir/diag.sh tcpflood -m100 -y
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 99
exit_test
