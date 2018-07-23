#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[discard-rptdmsg.sh\]: testing discard-rptdmsg functionality
. $srcdir/diag.sh init
startup_vg discard-rptdmsg.conf
. $srcdir/diag.sh tcpflood -m10 -i1
# we need to give rsyslog a little time to settle the receiver
./msleep 1500
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
seq_check 2 10
exit_test
