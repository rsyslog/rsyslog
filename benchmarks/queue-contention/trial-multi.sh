#!/bin/bash
# Eight imtcp workers consume concurrent TCP connections and submit to a
# four/eight-worker FixedArray main queue. tcpflood -Y uses one sending thread
# per connection and disjoint message-ID ranges; its supervised completion and
# exact final IDs prove the generators and all daemon workers completed.
# Timing separates startup/shutdown from generation-plus-drain work. The output
# line count is the drain barrier; no fixed sleep participates in the oracle.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=${BENCH_MESSAGES:-1000000}
: "${BENCH_INPUT_WORKERS:=8}" "${BENCH_CONSUMER_WORKERS:=4}"
: "${BENCH_CONNECTIONS:=16}" "${BENCH_PAYLOAD:=512}"
PORT_FILE="$PWD/$RSYSLOG_DYNNAME.input.port"
generate_conf
add_conf '
global(processInternalMessages="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
    listenPortFileName="'$PORT_FILE'" workerThreads="'$BENCH_INPUT_WORKERS'")
main_queue(queue.type="FixedArray" queue.size="32768"
    queue.workerThreads="'$BENCH_CONSUMER_WORKERS'" queue.workerThreadMinimumMessages="1024"
    queue.dequeueBatchSize="1024")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    set $!payload = parse_json("{\"nested\":{\"array\":[1,2,3,4,5,6,7,8],\"text\":\"queue contention benchmark payload\"}}");
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
start_ns=$(date +%s%N)
startup
assign_file_content INPUT_PORT "$PORT_FILE"
work_start_ns=$(date +%s%N)
tcpflood -p"$INPUT_PORT" -c"$BENCH_CONNECTIONS" -Y -m"$NUMMESSAGES" -d"$BENCH_PAYLOAD" >/dev/null
generator_end_ns=$(date +%s%N)
wait_file_lines --delay 10 "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" 120
work_end_ns=$(date +%s%N)
shutdown_when_empty
wait_shutdown
end_ns=$(date +%s%N)
seq_check
printf '{"lifecycle_ns":%d,"work_ns":%d,"generator_ns":%d,"drain_ns":%d}\n' \
    "$((end_ns - start_ns))" "$((work_end_ns - work_start_ns))" \
    "$((generator_end_ns - work_start_ns))" "$((work_end_ns - generator_end_ns))" > "$BENCH_METRIC_FILE"
exit_test
