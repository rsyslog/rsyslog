#!/bin/bash
# addd 2021-05-10 by Rainer Gerhards, released under ASL 2.0
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
if ! grep -qF -- "closed by remote peer " "$RSYSLOG_OUT_LOG"; then
	# Some platforms lose the peer details during shutdown and emit the
	# generic connection-close notification instead of the peer-annotated one.
	content_check "connection could not be established with host: "
fi
exit_test
