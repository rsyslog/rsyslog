#!/bin/bash
# check that we are able to receive messages from allowed sender
# added 2019-08-15 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5 # it's just important that we get any messages at all
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="rs")

$AllowedSender TCP,128.66.0.0/16 # this IP range is reserved by RFC5737
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="rs") {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_DYNNAME.must-not-be-created'")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -m$NUMMESSAGES
shutdown_when_empty
wait_shutdown
content_check --regex "connection request from disallowed sender .* discarded"
check_file_not_exists "$RSYSLOG_DYNNAME.must-not-be-created"
exit_test
