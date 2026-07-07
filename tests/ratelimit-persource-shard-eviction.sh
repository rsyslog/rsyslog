#!/bin/bash
## Regression test for sharded per-source ratelimit state eviction.
## The policy sets maxStates=1, which becomes an approximate shard-local cap.
## Sending many one-shot source keys forces shard-local LRU eviction. The
## oracle checks that eviction is visible through impstats and that an
## override-only key survives churn before later becoming active state.
## The short wait gives impstats one interval to publish the eviction counter
## before synchronized shutdown.

. ${srcdir:=.}/diag.sh init
require_plugin impstats

export STATSFILE="$RSYSLOG_DYNNAME.stats"
POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.persource-shard-eviction.yaml"
INPUT_FILE="$(pwd)/${RSYSLOG_DYNNAME}.input"
export POLICY_FILE INPUT_FILE

cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "PerSourceKey"
  maxStates: 1
  default:
    max: 1
    window: 60s
  overrides:
    - key: "specialhost"
      max: 5
      window: 60s
EOF

generate_conf
add_conf '
template(name="PerSourceKey" type="string" string="%hostname%")
ratelimit(name="per_source" policy="'$POLICY_FILE'")

module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
input(type="imtcp"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcp.port"
      ratelimit.name="per_source")

template(name="outfmt" type="string" string="host=%hostname% msg=%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

tcp_port=$(cat "$RSYSLOG_DYNNAME.tcp.port")

: > "$INPUT_FILE"
for i in $(seq 1 96); do
    echo "<13>Jan 1 00:00:00 churn$i app: msgnum:$i" >> "$INPUT_FILE"
done
for i in $(seq 1 3); do
    echo "<13>Jan 1 00:00:00 specialhost app: msgnum:$i" >> "$INPUT_FILE"
done

tcpflood -p"$tcp_port" -I "$INPUT_FILE"

sleep 2
shutdown_when_empty
wait_shutdown

special_count=$(grep -c "host=specialhost" "$RSYSLOG_OUT_LOG")
evicted_count=$(grep "ratelimit.per_source.per_source" "$STATSFILE" |
    grep -o "per_source_evicted=[0-9]*" | tail -1 | sed 's/.*=//')

echo "special_count: $special_count"
echo "evicted_count: $evicted_count"

if [ "$special_count" -ne 3 ]; then
    echo "FAIL: override-only specialhost entry was not preserved until activation"
    echo "output content:"
    cat "$RSYSLOG_OUT_LOG"
    error_exit 1
fi

if [ "${evicted_count:-0}" -le 0 ]; then
    echo "FAIL: expected per_source_evicted counter after shard-local churn"
    echo "stats content:"
    cat "$STATSFILE"
    error_exit 1
fi

exit_test
