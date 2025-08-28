#!/bin/bash
# add 2021-05-10 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	notifyonconnectionopen="on" notifyonconnectionclose="on")

:msg, contains, "msgnum:" {
	action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")

'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -m1 -M"\"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\""
shutdown_when_empty
wait_shutdown
content_check "connection established with "
content_check "closed by remote peer "
exit_test
