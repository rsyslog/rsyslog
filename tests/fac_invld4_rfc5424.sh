#!/bin/bash
# added 2014-10-01 by Rgerhards

# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
startup fac_invld4_rfc5424.conf
. $srcdir/diag.sh tcpflood -y -m1000 -P 8000000000000000000000000000000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 999 
exit_test
