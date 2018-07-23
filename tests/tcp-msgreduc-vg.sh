#!/bin/bash
# check if valgrind violations occur. Correct output is not checked.
# added 2011-03-01 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[tcp-msgreduc-vg.sh\]: testing msg reduction via UDP
. $srcdir/diag.sh init
startup_vg tcp-msgreduc-vg.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh tcpflood -t 127.0.0.1 -m 4 -r -M "\"<133>2011-03-01T11:22:12Z host tag msgh ...\""
. $srcdir/diag.sh tcpflood -t 127.0.0.1 -m 1 -r -M "\"<133>2011-03-01T11:22:12Z host tag msgh ...x\""
# we need to give rsyslog a little time to settle the receiver
./msleep 1500
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
exit_test
