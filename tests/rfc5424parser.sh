# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2013-11-22
echo ===============================================================================
echo \[rfc5424parser.sh\]: testing mmpstrucdata
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rfc5424parser.conf
sleep 1
source $srcdir/diag.sh tcpflood -m100 -y
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 99
source $srcdir/diag.sh exit
