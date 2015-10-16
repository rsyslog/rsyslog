#!/bin/bash
# check if valgrind violations occur. Correct output is not checked.
# added 2011-03-01 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[udp-msgreduc-orgmsg-vg.sh\]: testing msg reduction via udp, with org message
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg udp-msgreduc-orgmsg-vg.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh tcpflood -t 127.0.0.1 -m 4 -r -Tudp -M "\"<133>2011-03-01T11:22:12Z host tag msgh ...\""
. $srcdir/diag.sh tcpflood -t 127.0.0.1 -m 1 -r -Tudp -M "\"<133>2011-03-01T11:22:12Z host tag msgh ...x\""
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown-vg
if [ "$RSYSLOGD_EXIT" -eq "10" ]
then
	echo "udp-msgreduc-orgmsg-vg.sh FAILED"
	exit 1
fi
. $srcdir/diag.sh exit
