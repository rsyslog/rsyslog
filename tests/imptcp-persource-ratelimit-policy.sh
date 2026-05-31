#!/bin/bash
## Test that imptcp inherits per-source limiting through ratelimitAddMsg().
## The policy key template is configured only in the ratelimit YAML file; the
## module should not compute or pass a per-source key itself.
## The impstats sleep gives the one-second stats interval time to publish the
## key-evaluation counters that prove tplToString and parser use for this
## custom hostname template.

. ${srcdir:=.}/diag.sh init
require_plugin impstats
skip_ARM "ratelimit timing flaky on ARM"
export NUMMESSAGES=10
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export STATSFILE="$RSYSLOG_DYNNAME.stats"

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
INPUT_FILE="$(pwd)/${RSYSLOG_DYNNAME}.input"
export POLICY_FILE INPUT_FILE

cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "PerSourceKey"
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
ratelimit(name="per_source" policy="'$POLICY_FILE'")

module(load="../plugins/imptcp/.libs/imptcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
input(type="imptcp"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.ptcp.port"
      ratelimit.name="per_source")

template(name="outfmt" type="string" string="host=%hostname% msg=%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

ptcp_port=$(cat "$RSYSLOG_DYNNAME.ptcp.port")

: > "$INPUT_FILE"
for i in $(seq 1 20); do
    echo "<13>Jan 1 00:00:00 noisyhost app: msgnum:$i" >> "$INPUT_FILE"
done
for i in $(seq 1 5); do
    echo "<13>Jan 1 00:00:00 quiethost app: msgnum:$i" >> "$INPUT_FILE"
done

./tcpflood -p"$ptcp_port" -I "$INPUT_FILE"

sleep 2
shutdown_when_empty
wait_shutdown

noisy_count=$(grep -c "host=noisyhost" "$RSYSLOG_OUT_LOG")
quiet_count=$(grep -c "host=quiethost" "$RSYSLOG_OUT_LOG")
tpl_evals=$(grep "ratelimit.per_source.per_source" "$STATSFILE" | grep -o "per_source_key_tpl_evals=[0-9]*" | tail -1 | sed 's/.*=//')
parse_evals=$(grep "ratelimit.per_source.per_source" "$STATSFILE" | grep -o "per_source_key_parse_evals=[0-9]*" | tail -1 | sed 's/.*=//')

echo "noisy_count: $noisy_count"
echo "quiet_count: $quiet_count"
echo "tpl_evals: $tpl_evals"
echo "parse_evals: $parse_evals"

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

if [ "${tpl_evals:-0}" -le 0 ]; then
    echo "FAIL: expected custom per-source template to use tplToString"
    error_exit 1
fi

if [ "${parse_evals:-0}" -le 0 ]; then
    echo "FAIL: expected custom hostname template to parse messages before evaluation"
    error_exit 1
fi

exit_test
