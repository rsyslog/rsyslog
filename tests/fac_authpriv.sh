#!/bin/bash
# This tests proper processing of the authpriv facility.
# added 2014-09-16 by Rgerhards

# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
startup fac_authpriv.conf
. $srcdir/diag.sh tcpflood -m1000 -P 81
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 999 
exit_test
