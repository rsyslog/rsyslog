#!/bin/bash
# Test imptcp with large messages
# added 2010-08-10 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20000
generate_conf
add_conf '
global(maxMessageSize="10k")
template(name="outfmt" type="string" string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
# send messages of 10.000bytes plus header max, randomized
tcpflood -c5 -m$NUMMESSAGES -r -d10000
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -E
exit_test
