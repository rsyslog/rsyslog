#!/bin/bash
# adddd 2016-06-08 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'
startup
. $srcdir/diag.sh tcpflood -Trelp-plain -c-2000 -p13514 -m100000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 99999
exit_test
