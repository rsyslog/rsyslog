#!/bin/bash
# Test named rate limits for imudp
# This uses tcpflood -Tudp to send messages directly to imudp.

. ${srcdir:=.}/diag.sh init
skip_ARM "ratelimit timing flaky on ARM"
export SENDMESSAGES=20
export NUMMESSAGES=5
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines # ensure we wait for expected messages to arrive
# Receiver PORT
export PORT_RCVR="$(get_free_port)"

generate_conf
add_conf '
ratelimit(name="imudp_test_limit" interval="2" burst="5")

module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="'$PORT_RCVR'" ratelimit.name="imudp_test_limit")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then 
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

# Send 20 messages via UDP
./tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES

shutdown_when_empty
wait_shutdown

# Verify
content_count=$(grep -c "msgnum:" $RSYSLOG_OUT_LOG)
echo "content_count: $content_count"

# Burst is 5, sent 20. We expect limiting.
# Expected: ~5 messages.
# If we get all 20, ratelimit failed.
# If we get 0, everything failed.

if [ $content_count -eq $SENDMESSAGES ]; then
    echo "FAIL: No rate limiting occurred (received all $content_count messages)"
    error_exit 1
fi

if [ $content_count -eq 0 ]; then
     echo "FAIL: All messages lost or dropped (received 0)"
     error_exit 1
fi

echo "SUCCESS: Rate limiting occurred (received $content_count/$SENDMESSAGES)"
exit_test
