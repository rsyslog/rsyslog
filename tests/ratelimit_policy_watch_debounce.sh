#!/bin/bash
# Test debounce handling for inotify-based ratelimit policy reloads.

. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.imudp_port"
export POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export SENDMESSAGES=20

cat > "$POLICY_FILE" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML

generate_conf
add_conf '
global(processInternalMessages="on")
ratelimit(name="debounce_limiter" policy="'$POLICY_FILE'" policyWatch="on" policyWatchDebounce="500ms")
module(load="../plugins/imudp/.libs/imudp" batchSize="1")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'"
      ratelimit.name="debounce_limiter" ruleset="main")

template(name="outfmt" type="string" string="RECEIVED RAW: %rawmsg%\n")

ruleset(name="main") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

cat > "$POLICY_FILE" <<'YAML'
interval: 1
burst: 200
severity: 0
YAML
./msleep 100
cat > "$POLICY_FILE" <<'YAML'
interval: 1
burst: 100
severity: 0
YAML
./msleep 100
cat > "$POLICY_FILE" <<'YAML'
interval: 10
burst: 0
severity: 0
YAML

./msleep 1500

./tcpflood -Tudp -p$PORT_RCVR -m $SENDMESSAGES -M "msgnum:"
./msleep 1000
wait_queueempty

if [ -f "$RSYSLOG_OUT_LOG" ]; then
	content_count=$(grep -c "msgnum:" "$RSYSLOG_OUT_LOG" || true)
else
	content_count=0
fi
if [ "$content_count" -ne 0 ]; then
    echo "FAIL: final debounced policy expected 0 messages, got $content_count"
    error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
