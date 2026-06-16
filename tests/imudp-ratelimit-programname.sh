#!/bin/bash
## Regression test for imudp interval ratelimiting preserving parsed identity
## properties. The rate limiter consults APP-NAME for diagnostics before the
## message reaches the main queue; for RFC5424 messages this must not cache an
## empty derived programname before normal parsing. The input allows only one
## message per interval and receives two messages, so the test exercises the
## interval limiter and its drop path. The oracle is the final omfile output
## after shutdown, proving the accepted message reached its configured
## destination with both programname and app-name intact.

. ${srcdir:=.}/diag.sh init
skip_ARM "ratelimit timing flaky on ARM"
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.imudp_port"

generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'"
      ratelimit.interval="5" ratelimit.burst="1")

template(name="outfmt" type="string" string="prog=[%programname%] app=[%app-name%] msg=[%msg%]\n")

if $msg contains "rate-limit identity" then {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

./tcpflood -Tudp -p"$PORT_RCVR" -m2 -M "<29>1 2026-02-19T13:00:00.000Z router1 rpd 1234 TEST_MSG - rate-limit identity"

shutdown_when_empty
wait_shutdown

content_check "prog=[rpd] app=[rpd] msg=[rate-limit identity]"
exit_test
