#!/bin/bash
# addd 2017-03-31 by Pascal Withopf, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514")

:msg, contains, "msgnum:" {
	action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)
}

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)

'
startup
. $srcdir/diag.sh tcpflood -m1 -M"\"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\""
shutdown_when_empty
wait_shutdown

grep "imptcp: connection established" rsyslog.out.log > /dev/null
if [ $? -eq 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	error_exit 1
fi

grep "imptcp: session on socket.* closed" rsyslog.out.log > /dev/null
if [ $? -eq 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	error_exit 1
fi

exit_test
