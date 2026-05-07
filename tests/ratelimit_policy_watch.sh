#!/bin/bash
# Test inotify-based reload of external rate limit policies, including
# rename-based atomic replacement.

. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.imudp_port"
export POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export POLICY_TMP="$(pwd)/${RSYSLOG_DYNNAME}.policy.tmp.yaml"
export SENDMESSAGES=20

cat > "$POLICY_FILE" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML

generate_conf
add_conf '
global(processInternalMessages="on")
ratelimit(name="watch_limiter" policy="'$POLICY_FILE'" policyWatch="on" policyWatchDebounce="200ms")
module(load="../plugins/imudp/.libs/imudp" batchSize="1")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'"
      ratelimit.name="watch_limiter" ruleset="main")

template(name="outfmt" type="string" string="RECEIVED RAW: %rawmsg%\n")

ruleset(name="main") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

./tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"
wait_file_lines "$RSYSLOG_OUT_LOG" 20 100

: > "$RSYSLOG_OUT_LOG"

cat > "$POLICY_FILE" <<'YAML'
interval: 10
burst: 0
severity: 0
YAML
./msleep 1500

./tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"
./msleep 1000
wait_queueempty

content_count=$(grep -c "msgnum:" "$RSYSLOG_OUT_LOG" || true)
if [ "$content_count" -ne 0 ]; then
    echo "FAIL: watch reload expected 0 messages after restrictive update, got $content_count"
    error_exit 1
fi

cat > "$POLICY_TMP" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML
mv -f "$POLICY_TMP" "$POLICY_FILE"
./msleep 1500

./tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"
wait_file_lines "$RSYSLOG_OUT_LOG" 20 100

shutdown_when_empty
wait_shutdown
exit_test
