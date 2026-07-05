#!/bin/bash
# Verify that a zero-length $AllowedSender TCP netmask cannot become an
# allow-all ACL. runtime/net.c warns that /0 would match every sender and
# clamps the entry to a host mask; this test uses an RFC5737 address with /0
# and sends from localhost. The oracle is the receiver-side disallowed-sender
# diagnostic after a checked tcpflood attempt and the absence of the ruleset
# output file, proving the rejected connection did not submit a message.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="rs")

$AllowedSender TCP,128.66.0.0/0
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="rs") {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_DYNNAME.must-not-be-created'")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
tcpflood --check-only -p$TCPFLOOD_PORT -m$NUMMESSAGES
shutdown_when_empty
wait_shutdown
content_check --regex "connection request from disallowed sender .* discarded"
check_file_not_exists "$RSYSLOG_DYNNAME.must-not-be-created"
exit_test
