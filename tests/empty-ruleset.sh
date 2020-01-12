#!/bin/bash
# Copyright 2014-11-20 by Rainer Gerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
custom_wait_file_lines() {
	wait_file_lines "$RSYSLOG_OUT_LOG" 10000
}
export QUEUE_EMPTY_CHECK_FUNC=custom_wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
$MainMsgQueueTimeoutShutdown 10000

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="real")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2" ruleset="empty")

$template outfmt,"%msg:F,58:2%\n"

ruleset(name="empty") {
}

ruleset(name="real") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
assign_tcpflood_port2 "${RSYSLOG_DYNNAME}.tcpflood_port2"
tcpflood -p$TCPFLOOD_PORT2 -m5000 -i0 # these should NOT show up
tcpflood -p$TCPFLOOD_PORT -m10000 -i5000
tcpflood -p$TCPFLOOD_PORT2 -m500 -i15000 # these should NOT show up
shutdown_when_empty
wait_shutdown
seq_check 5000 14999
exit_test
