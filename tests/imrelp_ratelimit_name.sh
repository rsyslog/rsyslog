#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Test named rate limits for imrelp
# added 2025-07-15 by Copilot, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_ARM "ratelimit timing flaky on ARM"
export SENDMESSAGES=20
export NUMMESSAGES=5 # used by wait_file_lines (QUEUE_EMPTY_CHECK_FUNC); matches burst
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
ratelimit(name="imrelp_test_limit" interval="2" burst="5")

module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$TCPFLOOD_PORT'" ratelimit.name="imrelp_test_limit")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

tcpflood -Trelp-plain -p$TCPFLOOD_PORT -m $SENDMESSAGES

shutdown_when_empty
wait_shutdown

content_count=$(grep -c "msgnum:" $RSYSLOG_OUT_LOG)
echo "content_count: $content_count"

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
