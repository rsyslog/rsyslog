#!/bin/bash
## Repeated-message reduction can emit a summary for already accepted messages
## when the next distinct message arrives. If that distinct message is then
## dropped by per-source rate limiting, the summary must still be submitted.
## The oracle checks that only the first original message and the repeat
## summary are written; the distinct over-limit message must not appear.

. ${srcdir:=.}/diag.sh init

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.persource-repeat.yaml"
export POLICY_FILE
INPUT_FILE="$(pwd)/${RSYSLOG_DYNNAME}.input"
export INPUT_FILE

cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "PerSourceKey"
  default:
    max: 1
    window: 60s
EOF

generate_conf
add_conf '
$RepeatedMsgReduction on

template(name="PerSourceKey" type="string" string="%hostname%")

ratelimit(name="per_source" policy="'$POLICY_FILE'")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcp.port"
      ratelimit.name="per_source")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "repeated payload" or $msg contains "distinct over limit" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup

tcp_port=$(cat "$RSYSLOG_DYNNAME.tcp.port")

: > "$INPUT_FILE"
for _ in $(seq 1 4); do
    echo "<13>Jan 1 00:00:00 noisyhost app: repeated payload" >> "$INPUT_FILE"
done
echo "<13>Jan 1 00:00:00 noisyhost app: distinct over limit" >> "$INPUT_FILE"

tcpflood -p"$tcp_port" -I "$INPUT_FILE"

shutdown_when_empty
wait_shutdown

line_count=$(wc -l < "$RSYSLOG_OUT_LOG")
echo "line_count: $line_count"

if [ "$line_count" -ne 2 ]; then
    echo "FAIL: expected original message plus repeat summary only"
    cat "$RSYSLOG_OUT_LOG"
    error_exit 1
fi

if ! grep -q "repeated payload" "$RSYSLOG_OUT_LOG"; then
    echo "FAIL: original repeated payload missing"
    cat "$RSYSLOG_OUT_LOG"
    error_exit 1
fi

if ! grep -q "message repeated 3 times" "$RSYSLOG_OUT_LOG"; then
    echo "FAIL: repeat summary missing"
    cat "$RSYSLOG_OUT_LOG"
    error_exit 1
fi

if grep -q "distinct over limit" "$RSYSLOG_OUT_LOG"; then
    echo "FAIL: over-limit distinct message was submitted"
    cat "$RSYSLOG_OUT_LOG"
    error_exit 1
fi

exit_test
