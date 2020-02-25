#!/bin/bash
# add 2018-06-29 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset1" name="l1")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2" ruleset="ruleset1" name="l2")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port3" ruleset="ruleset1" name="l3")

template(name="outfmt" type="string" string="%inputname%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
assign_rs_port "$RSYSLOG_DYNNAME.tcpflood_port3"
tcpflood -p $TCPFLOOD_PORT -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 %PIX-7-710005: MSG\""
tcpflood -p $TCPFLOOD_PORT2 -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 %PIX-7-710005: MSG\""
tcpflood -p $RS_PORT -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 %PIX-7-710005: MSG\""
shutdown_when_empty
wait_shutdown

export EXPECTED='l1
l2
l3'
cmp_exact
exit_test
