#!/bin/bash
# Verify omrelp action-local ratelimit.interval/ratelimit.burst throttles RELP forwarding.
# A sender instance injects 20 messages quickly into one omrelp action with burst 5 and
# interval 2s. The receiver must get some, but not all, messages; that oracle proves the
# limiter drops only excess output for this destination instead of blocking delivery entirely.
. ${srcdir:=.}/diag.sh init
skip_ARM "ratelimit timing flaky on ARM"
export SENDMESSAGES=20

########## receiver ##########
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" address="127.0.0.1" port="0")

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_single_tcp_listener_port PORT_RCVR

########## sender ##########
generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")

action(type="omrelp" target="127.0.0.1" port="'$PORT_RCVR'"
       ratelimit.interval="2" ratelimit.burst="5")
' 2
startup 2

injectmsg2 0 $SENDMESSAGES

shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

if [ -f "$RSYSLOG_OUT_LOG" ]; then
	content_count=$(wc -l < "$RSYSLOG_OUT_LOG")
else
	content_count=0
fi

if [ "$content_count" -eq "$SENDMESSAGES" ]; then
	echo "FAIL: No rate limiting occurred (received all $content_count messages)"
	error_exit 1
fi

if [ "$content_count" -eq 0 ]; then
	echo "FAIL: All messages were lost (received 0)"
	error_exit 1
fi

echo "SUCCESS: omrelp rate limiting occurred (received $content_count/$SENDMESSAGES)"
exit_test
