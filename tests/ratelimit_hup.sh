#!/bin/bash
# Test HUP reloading of external rate limit policies
# 1. Start with high limit (pass all)
# 2. HUP to low limit (drop all)

. ${srcdir:=.}/diag.sh init

# Define ports and files
export PORT_RCVR="$(get_free_port)"
export POLICY_FILE="$(pwd)/test_policy_hup.yaml"

# Create initial policy (High limits)
echo "interval: 1" > $POLICY_FILE
echo "burst: 1000" >> $POLICY_FILE
echo "severity: 0" >> $POLICY_FILE

generate_conf
add_conf '
ratelimit(name="hup_limiter" policy="'$POLICY_FILE'")
module(load="../plugins/imudp/.libs/imudp" batchSize="1")
input(type="imudp" port="'$PORT_RCVR'" ratelimit.name="hup_limiter")

template(name="outfmt" type="string" string="RECEIVED RAW: %rawmsg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

# Phase 1: High Limit (Burst 1000)
# Send 20 messages. All should pass.
export SENDMESSAGES=20
echo "Checking ports for $PORT_RCVR:"
ss -ulpn | grep $PORT_RCVR
./tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"

# Allow time for processing
./msleep 3000

# Verify Phase 1
content_count=$(grep -c "msgnum:" $RSYSLOG_OUT_LOG)
echo "Phase 1 count: $content_count"
if [ $content_count -lt 20 ]; then
    echo "FAIL: Phase 1 expected 20 messages, got $content_count"
    error_exit 1
fi

# Reset log file for Phase 2 (clean slate)
> $RSYSLOG_OUT_LOG

# Phase 2: Restrictive Limit (Burst 0 - Drop All)
echo "Updating policy..."
echo "interval: 10" > $POLICY_FILE
echo "burst: 0" >> $POLICY_FILE
echo "severity: 0" >> $POLICY_FILE

echo "Sending HUP..."
issue_HUP
echo "Checking rsyslog process:"
ps aux | grep rsyslogd | grep -v grep

./msleep 1000 # wait for HUP to be processed

# Send 20 messages. 0 should pass.
./tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"

# Allow time for processing
./msleep 1000
wait_queueempty

# Verify Phase 2
content_count=$(grep -c "msgnum:" $RSYSLOG_OUT_LOG)
echo "Phase 2 count: $content_count"

# Note: Depending on race conditions during HUP, maybe 1 gets through?
# But with burst 0, strictly 0 should pass if updated.
if [ $content_count -ne 0 ]; then
    echo "FAIL: Phase 2 expected 0 messages (blocked), got $content_count"
    error_exit 1
fi

echo "SUCCESS: HUP reload updated rate limit policy"
exit_test
