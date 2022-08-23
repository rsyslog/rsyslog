#!/bin/bash
# check that we are able to receive messages from allowed sender
# added 2019-08-19 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
# Note: we need to know if CI supports DNS resolution. For this, we try to access
# our rsyslog echo service. If that works, DNS obviously works. On the contrary
# if we cannot access it, DNS might still work. But we prefer to skip the test in
# that case (on rsyslog CI it will always work).
rsyslog_testbench_test_url_access http://testbench.rsyslog.com/testbench/echo-get.php
export NUMMESSAGES=5 # it's just important that we get any messages at all
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="rs")

$AllowedSender TCP,www.rsyslog.com
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
