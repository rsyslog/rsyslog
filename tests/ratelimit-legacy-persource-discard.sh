#!/bin/bash
## Regression test for legacy split per-source ratelimit configuration.
## `perSource=on` plus `perSourcePolicy` and `perSourceKeyTpl` must still be
## enforced through the unified ratelimitAddMsg() path. The oracle checks both
## delivered output and imtcp's submitted counter so per-source drops keep the
## same discard semantics as the regular ratelimiter.
## The impstats sleep gives the one-second stats interval time to publish that
## submitted counter before shutdown.

. ${srcdir:=.}/diag.sh init
require_plugin impstats

export STATSFILE="$RSYSLOG_DYNNAME.stats"
POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.legacy-persource.yaml"
INPUT_FILE="$(pwd)/${RSYSLOG_DYNNAME}.input"
export POLICY_FILE INPUT_FILE

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
ratelimit(name="legacy_per_source"
          perSource="on"
          perSourcePolicy="'$POLICY_FILE'"
          perSourceKeyTpl="PerSourceKey")

module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
input(type="imtcp"
      name="legacy-persource"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcp.port"
      ratelimit.name="legacy_per_source")

template(name="outfmt" type="string" string="host=%hostname% msg=%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

tcp_port=$(cat "$RSYSLOG_DYNNAME.tcp.port")

: > "$INPUT_FILE"
for i in $(seq 1 20); do
    echo "<13>Jan 1 00:00:00 noisyhost app: msgnum:$i" >> "$INPUT_FILE"
done
for i in $(seq 1 5); do
    echo "<13>Jan 1 00:00:00 quiethost app: msgnum:$i" >> "$INPUT_FILE"
done

./tcpflood -p"$tcp_port" -I "$INPUT_FILE"

sleep 2
shutdown_when_empty
wait_shutdown

noisy_count=$(grep -c "host=noisyhost" "$RSYSLOG_OUT_LOG")
quiet_count=$(grep -c "host=quiethost" "$RSYSLOG_OUT_LOG")
content_count=$((noisy_count + quiet_count))
submitted_count=$(grep "origin=imtcp" "$STATSFILE" | grep -o "submitted=[0-9]*" | sed 's/.*=//' | sort -n | tail -1)

echo "noisy_count: $noisy_count"
echo "quiet_count: $quiet_count"
echo "submitted_count: $submitted_count"

if [ "$quiet_count" -ne 5 ]; then
    echo "FAIL: expected 5 quiethost messages, got $quiet_count"
    error_exit 1
fi

if [ "$noisy_count" -ge 20 ]; then
    echo "FAIL: legacy per-source ratelimit did not drop noisyhost messages"
    error_exit 1
fi

if [ "$noisy_count" -le 0 ]; then
    echo "FAIL: legacy per-source ratelimit dropped all noisyhost messages"
    error_exit 1
fi

if [ "${submitted_count:-0}" -ne "$content_count" ]; then
    echo "FAIL: imtcp submitted counter counted legacy per-source-dropped messages"
    echo "stats content:"
    cat "$STATSFILE"
    error_exit 1
fi

exit_test
