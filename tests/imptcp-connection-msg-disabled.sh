#!/bin/bash
# addd 2017-03-31 by Pascal Withopf, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514")

:msg, contains, "msgnum:" {
	action(type="omfile" file="rsyslog2.out.log")
}

action(type="omfile" file="rsyslog.out.log")

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M"\"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "imptcp: connection established" rsyslog.out.log > /dev/null
if [ $? -eq 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	. $srcdir/diag.sh error-exit 1
fi

grep "imptcp: session on socket.* closed" rsyslog.out.log > /dev/null
if [ $? -eq 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	. $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
