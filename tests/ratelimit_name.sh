#!/bin/bash
# Test named rate limits for imtcp and imptcp
# This test defines a rate limit policy and applies it to listeners.
. ${srcdir:=.}/diag.sh init --suppress-abort-on-error
skip_ARM "ratelimit timing flaky on ARM"
export NUMMESSAGES=10
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
ratelimit(name="test_limit" interval="2" burst="5")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcp.port" ratelimit.name="test_limit")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.ptcp.port" ratelimit.name="test_limit")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
# Send messages to TCP port
tcp_port=$(cat $RSYSLOG_DYNNAME.tcp.port)
./tcpflood -p$tcp_port -m 20
# Send messages to PTCP port
ptcp_port=$(cat $RSYSLOG_DYNNAME.ptcp.port)
./tcpflood -p$ptcp_port -m 20

shutdown_when_empty
wait_shutdown

# Check if messages were rate limited (we sent 20, burst is 5, interval 2s)
# We expect some loss.
content_count=$(grep -c "msgnum:" $RSYSLOG_OUT_LOG 2>/dev/null)
if [ -z "$content_count" ]; then
    content_count=0
fi
echo "content_count: $content_count"
if [ $content_count -eq 40 ]; then
    echo "FAIL: No rate limiting occurred (received all 40 messages)"
    error_exit 1
fi
if [ $content_count -lt 10 ]; then
     echo "FAIL: Too many messages lost (received $content_count)"
     error_exit 1
fi

echo "SUCCESS: Rate limiting occurred (received $content_count/40)"
exit_test
