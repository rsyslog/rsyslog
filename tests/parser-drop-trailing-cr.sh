#!/bin/bash
# Verify that parser.dropTrailingCROnReception removes an explicit carriage
# return left in the message payload after the normal LF framing byte is
# stripped. The oracle is the final omfile output after synchronized shutdown;
# without the option, the CR would be escaped as #015 in %msg%.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(parser.dropTrailingCROnReception="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="rs")

template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="rs") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
cr_msg=$(printf '"<167>Mar  6 16:57:54 172.20.245.8 test: payload\r"')
tcpflood -m1 -M "$cr_msg"
shutdown_when_empty
wait_shutdown

export EXPECTED=' payload'
cmp_exact

exit_test
