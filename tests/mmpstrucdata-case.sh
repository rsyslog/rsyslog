#!/bin/bash
# the goal here is to detect memleaks when structured data is not
# correctly parsed.
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2015-04-30
. $srcdir/diag.sh init

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

. $srcdir/diag.sh startup mmpstrucdata-case.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh tcpflood -m100 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 eventID=\\\"1011\\\"] valid structured data\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-check-with-count eventID 100
. $srcdir/diag.sh exit
