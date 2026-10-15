#!/bin/bash
# Exercise imtcp before/through a HUP reload boundary and prove exact delivery.
# Oracle: one session requires strict output order; 32 sessions require only no
# gaps/duplicates because cross-connection scheduling has no global order.
set -eu
: "${BENCH_BUILD_DIR:?}" "${BENCH_METRIC_FILE:?}" "${BENCH_PHASE:?}"
cd "$BENCH_BUILD_DIR/tests"
export srcdir="$BENCH_BUILD_DIR/tests"
export RS_REDIR=''
. "$srcdir/diag.sh" init
PORT_FILE="$PWD/${RSYSLOG_DYNNAME}.listen"
PERF_FILE="$PWD/${RSYSLOG_DYNNAME}.perf.csv"
RELOAD_LOG="$PWD/${RSYSLOG_DYNNAME}.reload.log"
STRACE_FILE="$PWD/${RSYSLOG_DYNNAME}.strace"
VG_FILE="$PWD/${RSYSLOG_DYNNAME}.valgrind"
LOCK_FILE="$PWD/${RSYSLOG_DYNNAME}.locks"
PERF_RECORD_FILE="$PWD/${RSYSLOG_DYNNAME}.perf.data"
DISASSEMBLY_FILE="$PWD/${RSYSLOG_DYNNAME}.rsyslogd.disassembly"
cleanup() { rm -f "$PORT_FILE" "$PERF_FILE" "$STRACE_FILE" "$VG_FILE" "$LOCK_FILE" "$PERF_RECORD_FILE"; }
trap cleanup EXIT
wait_reload_log() {
	local pattern=$1
	local offset=${2:-0}
	local tries=300
	while [ "$tries" -gt 0 ]; do
		if [ -f "$RELOAD_LOG" ] && tail -c +$((offset + 1)) "$RELOAD_LOG" | grep -q "$pattern"; then
			return 0
		fi
		tries=$((tries - 1))
		"$TESTTOOL_DIR/msleep" 100
	done
	return 1
}
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
template(name="benchOut" type="string" string="%msg%\n")
ruleset(name="reloadProbe") { stop }
'
if [ "$BENCH_PHASE" = reload ] && [ "$BENCH_ROLE" = candidate ]; then
	add_conf 'global(config.reloadOnHUP="validate")'
fi
if [ "$BENCH_RULESET" = named ]; then
	# shellcheck disable=SC2089
  if [ "$BENCH_QUEUE" = direct ]; then queue='queue.type="Direct"'; else queue='queue.type="LinkedList" queue.dequeueBatchSize="'$BENCH_BATCH_SIZE'" queue.workerThreads="1"'; fi
	# shellcheck disable=SC2090
  add_conf 'ruleset(name="bench" '$queue') { if $msg contains "msgnum:" then action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="benchOut") }'
  input_ruleset='ruleset="bench"'
else
  if [ "$BENCH_QUEUE" = direct ]; then add_conf 'main_queue(queue.type="Direct")'; else add_conf 'main_queue(queue.type="LinkedList" queue.dequeueBatchSize="'$BENCH_BATCH_SIZE'" queue.workerThreads="1")'; fi
  add_conf 'if $msg contains "msgnum:" then action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="benchOut")'
  input_ruleset=''
fi
add_conf 'input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$PORT_FILE"'" workerthreads="1" '"$input_ruleset"')'
prefix=''
# The controller is the daemon process.  Exactly one optional wrapper observes
# it, so perf/strace/Valgrind/lock results retain an unambiguous attribution.
if [ "$BENCH_PERF_STAT" = 1 ]; then prefix="perf stat -x, -o '$PERF_FILE' -e task-clock,cycles,instructions,branches,branch-misses,cache-misses,context-switches,cpu-migrations,page-faults --"; fi
if [ "$BENCH_STRACE_SUMMARY" = 1 ]; then prefix="strace -f -c -o '$STRACE_FILE'"; fi
if [ "$BENCH_ALLOCATION_SUMMARY" = 1 ]; then prefix="valgrind --tool=memcheck --log-file='$VG_FILE'"; fi
if [ "$BENCH_LOCK_REPORT" = 1 ]; then prefix="perf lock record -o '$LOCK_FILE.data' --"; fi
if [ "$BENCH_PERF_RECORD" = 1 ]; then prefix="perf record -g -o '$PERF_RECORD_FILE' --"; fi
# shellcheck disable=SC2034 # diag.sh consumes this dynamic controller wrapper.
if [ -n "$prefix" ]; then valgrind="$prefix"; fi
# Keep controller diagnostics separate from sender output and retain them as a
# raw artifact.  They are the sole evidence for capability detection.
# shellcheck disable=SC2034 # diag.sh consumes this dynamic controller redirection.
if [ "$BENCH_PHASE" = reload ]; then RS_REDIR="> $RELOAD_LOG 2>&1"; fi
if [ -n "${BENCH_CONFIG_ARTIFACT:-}" ]; then cp "$CONF_FILE" "${BENCH_CONFIG_ARTIFACT}.before.conf"; fi
if [ "$BENCH_DISASSEMBLY" = 1 ]; then objdump -d ../tools/rsyslogd >"$DISASSEMBLY_FILE" 2>&1 || :; fi
startup
assign_file_content INPUT_PORT "$PORT_FILE"
start_ns=$(date +%s%N)
reload_ns=0
socket_continuity=false
if [ "$BENCH_PHASE" = reload ]; then
	# -b1/-W keeps the same TCP connections sending while HUP is processed.
	# Seeing record zero first proves the sender is active before the transactional reload.
	tcpflood -p"$INPUT_PORT" -c"$BENCH_TCP_SESSIONS" -Y -m"$BENCH_MESSAGES" -d"$BENCH_PAYLOAD_BYTES" \
		-b1 -W"$BENCH_RELOAD_SEND_WAIT_US" >/dev/null &
	sender_pid=$!
	wait_file_lines "$RSYSLOG_OUT_LOG" 1 30
	rss_before=$(ps -o rss= -p "$(cat "$RSYSLOG_PIDBASE.pid")" | tr -d ' ')
	# Change a ruleset declaration in the replacement config. Validate mode must
	# confirm this candidate without swapping the active configuration.
	sed -i 's/reloadProbe/reloadProbeNext/' "$CONF_FILE"
	if [ -n "${BENCH_CONFIG_ARTIFACT:-}" ]; then cp "$CONF_FILE" "${BENCH_CONFIG_ARTIFACT}.candidate.conf"; fi
	reload_log_offset=$(wc -c < "$RELOAD_LOG")
	reload_start=$(date +%s%N)
	issue_HUP
	if [ "$BENCH_ROLE" = candidate ]; then
		wait_reload_log "shadow_reload event=request result=reported_only mode=validate" "$reload_log_offset" || \
			error_exit 1 "transactional validate HUP did not report the candidate diff"
	fi
	reload_ns=$(( $(date +%s%N) - reload_start ))
	rss_after=$(ps -o rss= -p "$(cat "$RSYSLOG_PIDBASE.pid")" | tr -d ' ')
	wait "$sender_pid"
	# tcpflood does not reconnect. Successful completion of the same process
	# therefore proves all established sockets survived the HUP boundary.
	socket_continuity=true
else
	tcpflood -p"$INPUT_PORT" -c"$BENCH_TCP_SESSIONS" -Y -m"$BENCH_MESSAGES" -d"$BENCH_PAYLOAD_BYTES" >/dev/null
fi
if [ "$BENCH_PHASE" != reload ]; then rss_before=0; rss_after=0; fi
sent_ns=$(date +%s%N)
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$BENCH_MESSAGES" 300
delivered_ns=$(date +%s%N)
shutdown_when_empty
wait_shutdown
if [ "$BENCH_LOCK_REPORT" = 1 ] && [ -f "$LOCK_FILE.data" ]; then perf lock report -i "$LOCK_FILE.data" >"$LOCK_FILE" 2>&1 || :; fi
mkdir -p "$(dirname "$BENCH_METRIC_FILE")"
# Preserve raw controller evidence beside its unique workload artifact.  These
# files are diagnostic evidence only; no optional collector is an acceptance gate.
for raw in "$PERF_FILE" "$STRACE_FILE" "$VG_FILE" "$LOCK_FILE" "$PERF_RECORD_FILE" "$DISASSEMBLY_FILE" "$RELOAD_LOG"; do
	[ -f "$raw" ] && cp "$raw" "$BENCH_METRIC_FILE.$(basename "$raw")"
done
cut -d: -f2 "$RSYSLOG_OUT_LOG" >"${RSYSLOG_OUT_LOG}.seq"
mv "${RSYSLOG_OUT_LOG}.seq" "$RSYSLOG_OUT_LOG"
export NUMMESSAGES="$BENCH_MESSAGES"
if [ "$BENCH_TCP_SESSIONS" -eq 1 ]; then
	awk -v expected="$BENCH_MESSAGES" '($0 + 0) != NR - 1 { bad=1 } END { exit (bad || NR != expected) ? 1 : 0 }' "$RSYSLOG_OUT_LOG" || \
		error_exit 1 "single TCP session violated strict delivery order"
	oracle=strict_order
else
	seq_check 0 $((BENCH_MESSAGES - 1))
	oracle=no_gaps_or_duplicates
fi
python3 - "$PERF_FILE" "$STRACE_FILE" "$VG_FILE" "$LOCK_FILE" "$BENCH_METRIC_FILE" <<'PY'
import json, re, sys
perf, strace, vg, locks, out = sys.argv[1:]
def read(path):
    try: return open(path, encoding='utf-8', errors='replace').read()
    except OSError: return ''
def perf_values(text):
    values = {}
    for line in text.splitlines():
        parts = line.split(',')
        if len(parts) >= 3 and parts[0].strip() not in ('', '<not supported>', '<not counted>'):
            try: values[parts[2].strip()] = float(parts[0].replace(' ', ''))
            except ValueError: pass
    return values
def syscall_values(text):
    values = {}
    for line in text.splitlines():
        fields=line.split()
        if len(fields) in (5,6) and fields[0][:1].isdigit() and fields[-1] != 'total':
            try: values[fields[-1]]={'calls':int(fields[3]), 'errors':int(fields[4]) if len(fields)==6 else 0}
            except ValueError: pass
    return values
heap = re.search(r'total heap usage: ([0-9,]+) allocs, ([0-9,]+) frees', read(vg))
data={'perf_stat': perf_values(read(perf)), 'syscalls': syscall_values(read(strace)),
      'allocations': int(heap.group(1).replace(',', '')) if heap else None,
      'lock_report_available': bool(read(locks))}
open(out + '.instrumentation', 'w').write(json.dumps(data))
PY
instrumentation=$(cat "$BENCH_METRIC_FILE.instrumentation")
printf '{"workload_id":"%s","payload_bytes":%d,"tcp_sessions":%d,"ruleset":"%s","queue":"%s","batch_size":%d,"phase":"%s","messages":%d,"execution":"completed","throughput_messages_per_second":%.3f,"cpu_seconds_per_message":null,"send_ns":%d,"delivery_latency_ns":%d,"reload_ns":%d,"rss_before_kib":%d,"rss_after_kib":%d,"hot_path_invariants":{"exact_delivery":true,"delivery_oracle":"%s","hup_processed":%s,"socket_continuity":%s},"instrumentation":%s}\n' \
 "$BENCH_WORKLOAD_ID" "$BENCH_PAYLOAD_BYTES" "$BENCH_TCP_SESSIONS" "$BENCH_RULESET" "$BENCH_QUEUE" "$BENCH_BATCH_SIZE" "$BENCH_PHASE" "$BENCH_MESSAGES" \
 "$(awk -v n="$BENCH_MESSAGES" -v t="$((delivered_ns-start_ns))" 'BEGIN{print n*1000000000/t}')" "$((sent_ns-start_ns))" "$((delivered_ns-sent_ns))" "$reload_ns" "$rss_before" "$rss_after" \
 "$oracle" "$( [ "$BENCH_PHASE" = reload ] && echo true || echo false )" "$socket_continuity" "$instrumentation" >"$BENCH_METRIC_FILE"
exit_test
