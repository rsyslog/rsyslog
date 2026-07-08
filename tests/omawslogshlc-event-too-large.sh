#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# Test that omawslogshlc rejects a rendered event over the CloudWatch HLC
# 256 KiB per-event limit locally, before attempting HTTP delivery. The oracle
# is the internal rsyslog diagnostic emitted after tcpflood sends one
# deliberately oversized message through a larger global(maxMessageSize)
# setting and a dynamically allocated imtcp listener.
. ${srcdir:=.}/diag.sh init

require_plugin "omawslogshlc"
require_plugin "imtcp"

generate_conf
add_conf '
global(processInternalMessages="on" internalmsg.severity="info" maxMessageSize="300k")

module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omawslogshlc/.libs/omawslogshlc")
template(name="omawslogshlc-plain" type="string" string="%msg%\n")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="send")

ruleset(name="send") {
	action(
		type="omawslogshlc"
		region="us-west-2"
		bearer_token="fake-token"
		log_group="test-group"
		log_stream="test-stream"
		template="omawslogshlc-plain"
		action.resumeRetryCount="0"
	)
}

*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
tcpflood -p"$TCPFLOOD_PORT" -m1 -d 270000 -P129
shutdown_when_empty
wait_shutdown

content_check "omawslogshlc: single event size"
content_check "exceeds 256 KiB limit"
check_not_present "omawslogshlc: HTTP"

exit_test
