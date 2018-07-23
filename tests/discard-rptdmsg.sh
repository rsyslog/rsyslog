#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[discard-rptdmsg.sh\]: testing discard-rptdmsg functionality
. $srcdir/diag.sh init
startup discard-rptdmsg.conf
. $srcdir/diag.sh tcpflood -m10 -i1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 2 10
exit_test
