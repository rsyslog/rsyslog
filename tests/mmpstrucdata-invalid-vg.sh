#!/bin/bash
# the goal here is to detect memleaks when structured data is not
# correctly parsed.
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2015-04-30

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[mmpstrucdata-invalid.sh\]: testing mmpstrucdata with invalid SD
. $srcdir/diag.sh init
startup_vg mmpstrucdata-invalid.conf
. $srcdir/diag.sh wait-startup
# we use different message counts as this hopefully aids us
# in finding which sample is leaking. For this, check the number
# of blocks lost and see what set they match.
. $srcdir/diag.sh tcpflood -m100 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 MSGNUM] invalid structured data!\""
. $srcdir/diag.sh tcpflood -m200 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 MSGNUM ] invalid structured data!\""
. $srcdir/diag.sh tcpflood -m300 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 MSGNUM= ] invalid structured data!\""
. $srcdir/diag.sh tcpflood -m400 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 = ] invalid structured data!\""
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
exit_test
