#!/bin/bash
# Test YAML ratelimit object policyWatch/policyWatchDebounce parsing and reload.

. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

export PORT_RCVR="$(get_free_port)"
export POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export SENDMESSAGES=20

cat > "$POLICY_FILE" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
module(load="../plugins/imudp/.libs/imudp" batchSize="1")
input(type="imudp" port="'$PORT_RCVR'" ratelimit.name="yaml_watch" ruleset="main")
'

cat > "${RSYSLOG_DYNNAME}.yaml" <<'YAMLEOF'
ratelimits:
  - name: yaml_watch
    policy: "${POLICY_FILE}"
    policyWatch: on
    policyWatchDebounce: 200ms

templates:
  - name: outfmt
    type: string
    string: "RECEIVED RAW: %rawmsg%\n"

rulesets:
  - name: main
    script: |
      action(type="omfile" file="${RSYSLOG_OUT_LOG}" template="outfmt")
YAMLEOF

sed -i \
    -e "s|\${POLICY_FILE}|${POLICY_FILE}|g" \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

startup

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
    echo "FAIL: YAML ratelimit policyWatch expected 0 messages after update, got $content_count"
    error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
