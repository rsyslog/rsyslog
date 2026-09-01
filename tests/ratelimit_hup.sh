#!/bin/bash
# Verify HUP reloading of an external input rate-limit policy: an initial high
# burst passes all messages, then HUP reloads a zero-burst policy that drops
# every subsequent message.  The oracle waits for rsyslog's policy-specific
# reload confirmation rather than relying on a fixed delay, because the HUP
# diagnostic handshake can time out before a slow or sanitizer-instrumented
# worker has reloaded the policy.

. ${srcdir:=.}/diag.sh init

# Define ports and files
export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.imudp_port"
POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.test_policy_hup.yaml"
export POLICY_FILE

wait_for_policy_reload() {
    local retries=0
    local reload_message="ratelimit: HUP reloaded policy 'hup_limiter'"

    while [ "$retries" -lt 40 ]; do
        if [ -f "${RSYSLOG_DYNNAME}.started" ] && grep -qF "$reload_message" "${RSYSLOG_DYNNAME}.started"; then
            return 0
        fi
        retries=$((retries + 1))
        ./msleep 250
    done

    echo "FAIL: timed out waiting for HUP reload of policy 'hup_limiter'"
    error_exit 1
}

# Create initial policy (High limits)
echo "interval: 1" > $POLICY_FILE
echo "burst: 1000" >> $POLICY_FILE
echo "severity: 0" >> $POLICY_FILE

generate_conf
add_conf '
ratelimit(name="hup_limiter" policy="'$POLICY_FILE'")
module(load="../plugins/imudp/.libs/imudp" batchSize="1")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'"
      ratelimit.name="hup_limiter")

template(name="outfmt" type="string" string="RECEIVED RAW: %rawmsg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

# Phase 1: High Limit (Burst 1000)
# Send 20 messages. All should pass.
export SENDMESSAGES=20
echo "Checking ports for $PORT_RCVR:"
ss -ulpn | grep $PORT_RCVR
tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"

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
: > "$RSYSLOG_OUT_LOG"

# Phase 2: Restrictive Limit (Burst 0 - Drop All)
echo "Updating policy..."
echo "interval: 10" > $POLICY_FILE
echo "burst: 0" >> $POLICY_FILE
echo "severity: 0" >> $POLICY_FILE

echo "Sending HUP..."
issue_HUP
wait_for_policy_reload
echo "Checking rsyslog process:"
ps aux | grep rsyslogd | grep -v grep

# Send 20 messages. 0 should pass.
tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"

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
