# check if valgrind violations occur. Correct output is not checked.
# added 2011-03-01 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[tcp-msgreduc-vg.sh\]: testing msg reduction via UDP
source $srcdir/diag.sh init
source $srcdir/diag.sh startup-vg tcp-msgreduc-vg.conf
source $srcdir/diag.sh wait-startup
./tcpflood -t 127.0.0.1 -m 4 -r -M "<133>2011-03-01T11:22:12Z host tag msgh ..."
./tcpflood -t 127.0.0.1 -m 1 -r -M "<133>2011-03-01T11:22:12Z host tag msgh ...x"
# we need to give rsyslog a little time to settle the receiver
./msleep 1500
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown-vg
source $srcdir/diag.sh check-exit-vg
source $srcdir/diag.sh exit
