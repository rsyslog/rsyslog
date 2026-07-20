#!/bin/bash
# Run one full rsyslog lifecycle and validate exact delivery.
: "${BENCH_BUILD_DIR:?}" "${BENCH_MODULE:?}" "${BENCH_KEY_MODE:?}"
: "${BENCH_CONNECTIONS:?}" "${BENCH_MESSAGES:?}" "${BENCH_PAYLOAD:?}"
: "${BENCH_MAX_STATES:?}" "${BENCH_CHURN_ROUNDS:?}" "${BENCH_METRIC_FILE:?}"

cd "$BENCH_BUILD_DIR/tests" || exit 1
export srcdir="$BENCH_BUILD_DIR/tests"
. "$srcdir/diag.sh" init

PORT_FILE="$PWD/${RSYSLOG_DYNNAME}.input.port"
POLICY_FILE="$PWD/${RSYSLOG_DYNNAME}.policy.yaml"
case "$BENCH_KEY_MODE" in
	port) key_template=BenchPort ;;
	ip-port) key_template=BenchIpPort ;;
	host-port) key_template=BenchHostPort ;;
	*) error_exit 1 "unknown key mode $BENCH_KEY_MODE" ;;
esac
cat >"$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "$key_template"
  maxStates: $BENCH_MAX_STATES
  default:
    max: $BENCH_MESSAGES
    window: 1h
EOF

generate_conf
add_conf '
main_queue(queue.workerThreads="1")
template(name="BenchPort" type="string" string="%fromhost-port%")
template(name="BenchIpPort" type="string" string="%fromhost-ip%:%fromhost-port%")
template(name="BenchHostPort" type="string" string="%fromhost%:%fromhost-port%")
ratelimit(name="bench" policy="'"$POLICY_FILE"'")
module(load="../plugins/'"$BENCH_MODULE"'/.libs/'"$BENCH_MODULE"'")
input(type="'"$BENCH_MODULE"'" address="127.0.0.1" port="0"
      listenPortFileName="'"$PORT_FILE"'" ratelimit.name="bench")
template(name="benchOut" type="string" string="%msg%\n")
if $msg contains "msgnum:" then
  action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="benchOut")
'

startup
assign_file_content INPUT_PORT "$PORT_FILE"
start_ns=$(date +%s%N)
per_round=$((BENCH_MESSAGES / BENCH_CHURN_ROUNDS))
offset=0
round=0
while [ "$round" -lt "$BENCH_CHURN_ROUNDS" ]; do
	tcpflood -p"$INPUT_PORT" -c"$BENCH_CONNECTIONS" -Y -m"$per_round" \
		-i"$offset" -d"$BENCH_PAYLOAD" >/dev/null
	offset=$((offset + per_round))
	round=$((round + 1))
done
end_ns=$(date +%s%N)
wait_file_lines "$RSYSLOG_OUT_LOG" "$BENCH_MESSAGES" 300
shutdown_when_empty
wait_shutdown
export NUMMESSAGES="$BENCH_MESSAGES"
cut -d: -f2 "$RSYSLOG_OUT_LOG" >"${RSYSLOG_OUT_LOG}.seq"
mv "${RSYSLOG_OUT_LOG}.seq" "$RSYSLOG_OUT_LOG"
seq_check 0 $((BENCH_MESSAGES - 1))
mkdir -p "$(dirname "$BENCH_METRIC_FILE")"
printf '{"module":"%s","key_mode":"%s","connections":%d,"messages":%d,"payload":%d,"max_states":%d,"churn_rounds":%d,"elapsed_ns":%d,"messages_per_second":%.3f}\n' \
	"$BENCH_MODULE" "$BENCH_KEY_MODE" "$BENCH_CONNECTIONS" "$BENCH_MESSAGES" \
	"$BENCH_PAYLOAD" "$BENCH_MAX_STATES" "$BENCH_CHURN_ROUNDS" "$((end_ns-start_ns))" \
	"$(awk -v n="$BENCH_MESSAGES" -v t="$((end_ns-start_ns))" 'BEGIN { print n * 1000000000 / t }')" \
	>"$BENCH_METRIC_FILE"
exit_test
