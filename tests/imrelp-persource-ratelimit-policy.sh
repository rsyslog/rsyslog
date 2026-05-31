#!/bin/bash
## Test that imrelp inherits per-source limiting through ratelimitAddMsg().
## The oracle also covers discard handling: rate-limited RELP messages must not
## make librelp treat the receive callback as a session-level failure.

. ${srcdir:=.}/diag.sh init
skip_ARM "ratelimit timing flaky on ARM"
export SENDMESSAGES=20
export NUMMESSAGES=5
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export POLICY_FILE

cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "PerSourceIP"
  default:
    max: 5
    window: 2s
EOF

generate_conf
add_conf '
template(name="PerSourceIP" type="string" string="%fromhost-ip%")
ratelimit(name="per_source" policy="'$POLICY_FILE'")

module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$TCPFLOOD_PORT'" ratelimit.name="per_source")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

tcpflood -Trelp-plain -p"$TCPFLOOD_PORT" -m "$SENDMESSAGES"

shutdown_when_empty
wait_shutdown

content_count=$(grep -c "msgnum:" "$RSYSLOG_OUT_LOG")
echo "content_count: $content_count"

if [ "$content_count" -eq "$SENDMESSAGES" ]; then
    echo "FAIL: per-source ratelimit did not drop RELP messages"
    error_exit 1
fi

if [ "$content_count" -eq 0 ]; then
    echo "FAIL: all RELP messages were lost or dropped"
    error_exit 1
fi

exit_test
