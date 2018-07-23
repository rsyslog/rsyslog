#!/bin/bash
# addd 2017-03-31 by Pascal Withopf, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" notifyonconnectionclose="on" notifyonconnectionopen="on")

:msg, contains, "msgnum:" {
	action(type="omfile" file="rsyslog2.out.log")
}

action(type="omfile" file="rsyslog.out.log")

'
startup
. $srcdir/diag.sh tcpflood -m1 -M"\"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\""
shutdown_when_empty
wait_shutdown

grep "imptcp: connection established" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	error_exit 1
fi

grep "imptcp: session on socket.* closed" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	error_exit 1
fi

exit_test
