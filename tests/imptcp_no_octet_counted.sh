# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_no_octet_counted.sh\]: test imptcp with octet counted framing disabled
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imptcp_no_octet_counted.conf
source $srcdir/diag.sh tcpflood -B -I testsuites/no_octet_counted.testdata
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 19
source $srcdir/diag.sh exit
