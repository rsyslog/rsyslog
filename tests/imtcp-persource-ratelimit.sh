#!/bin/bash
## Test per-source ratelimiting for imtcp using a YAML policy.

. ${srcdir:=.}/diag.sh init

export POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.persource.yaml"
export INPUT_FILE="$(pwd)/${RSYSLOG_DYNNAME}.input"

cat > "$POLICY_FILE" <<EOF
default:
  max: 5
  window: 2s
overrides:
  - key: "quiethost"
    max: 50
    window: 2s
EOF

generate_conf
add_conf '
template(name="PerSourceKey" type="string" string="%hostname%")

ratelimit(name="per_source" perSource="on" perSourcePolicy="'$POLICY_FILE'" perSourceKeyTpl="PerSourceKey")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcp.port"
      ratelimit.name="per_source")

template(name="outfmt" type="string" string="host=%hostname% msg=%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

tcp_port=$(cat $RSYSLOG_DYNNAME.tcp.port)

> "$INPUT_FILE"
for i in $(seq 1 20); do
    echo "<13>Jan 1 00:00:00 noisyhost app: msgnum:$i" >> "$INPUT_FILE"
done
for i in $(seq 1 5); do
    echo "<13>Jan 1 00:00:00 quiethost app: msgnum:$i" >> "$INPUT_FILE"
done

./tcpflood -p"$tcp_port" -I "$INPUT_FILE"

shutdown_when_empty
wait_shutdown

noisy_count=$(grep -c "host=noisyhost" "$RSYSLOG_OUT_LOG")
quiet_count=$(grep -c "host=quiethost" "$RSYSLOG_OUT_LOG")

echo "noisy_count: $noisy_count"
echo "quiet_count: $quiet_count"

if [ "$quiet_count" -ne 5 ]; then
    echo "FAIL: expected 5 quiethost messages, got $quiet_count"
    error_exit 1
fi

if [ "$noisy_count" -ge 20 ]; then
    echo "FAIL: per-source ratelimit did not drop noisyhost messages"
    error_exit 1
fi

if [ "$noisy_count" -le 0 ]; then
    echo "FAIL: per-source ratelimit dropped all noisyhost messages"
    error_exit 1
fi

exit_test
