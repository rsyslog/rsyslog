# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[discard-rptdmsg.sh\]: testing discard-rptdmsg functionality
source $srcdir/diag.sh init
source $srcdir/diag.sh startup-vg discard-rptdmsg.conf
source $srcdir/diag.sh tcpflood -m10 -i1
# we need to give rsyslog a little time to settle the receiver
./msleep 1500
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown-vg
source $srcdir/diag.sh check-exit-vg
source $srcdir/diag.sh seq-check 2 10
source $srcdir/diag.sh exit
