# Test for "stop" statement
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[stop.sh\]: testing stop statement
source $srcdir/diag.sh init
source $srcdir/diag.sh startup stop.conf
sleep 1
source $srcdir/diag.sh tcpflood -m10 -i1
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 2 10
source $srcdir/diag.sh exit
