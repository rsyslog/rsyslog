#!/bin/bash
# Full daemon lifecycle, with exact delivery checked outside the timed phase.
# Run from a configured build's tests directory using the standard testbench.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=${BENCH_MESSAGES:-100000}
generate_conf
add_conf '
global(processInternalMessages="off")
main_queue(queue.type="FixedArray" queue.size="32768"
    queue.workerThreads="4" queue.workerThreadMinimumMessages="1024"
    queue.dequeueBatchSize="1024")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    set $!payload = parse_json("{\"nested\":{\"array\":[1,2,3,4,5,6,7,8],\"text\":\"queue lifecycle benchmark payload\"}}");
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
start_ns=$(date +%s%N)
startup
injectmsg
shutdown_when_empty
wait_shutdown
end_ns=$(date +%s%N)
seq_check
printf '%s\n' "$((end_ns - start_ns))" > "$BENCH_METRIC_FILE"
exit_test
