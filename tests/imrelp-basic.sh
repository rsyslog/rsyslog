#!/bin/bash
# addd 2016-05-13 by RGerhards, released under ASL 2.0

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME RELP"
   exit 77
fi


. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -Trelp-plain -p13514 -m10000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 9999
. $srcdir/diag.sh exit
