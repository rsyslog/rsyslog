# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[discard-allmark.sh\]: testing discard-allmark functionality
source $srcdir/diag.sh init
source $srcdir/diag.sh startup discard-allmark.conf
source $srcdir/diag.sh tcpflood -m10 -i1
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 2 10
source $srcdir/diag.sh exit
