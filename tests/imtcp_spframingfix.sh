# This file is part of the rsyslog project, released  under ASL 2.0
echo ====================================================================================
echo TEST: \[imptcp_spframingfix.sh\]: test imptcp in regard to Cisco ASA framing fix
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imtcp_spframingfix.conf
source $srcdir/diag.sh tcpflood -B -I testsuites/spframingfix.testdata
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 19
source $srcdir/diag.sh exit
