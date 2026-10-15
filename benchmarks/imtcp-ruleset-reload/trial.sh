#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise imtcp before/through a HUP reload boundary and prove exact delivery.
# Oracle: one session requires strict output order; 32 sessions require only no
# gaps/duplicates because cross-connection scheduling has no global order.
set -e
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
cleanup() {
	if [ -n "${sender_pid:-}" ]; then
		kill "$sender_pid" 2>/dev/null || :
		wait "$sender_pid" 2>/dev/null || :
	fi
	# On failure, terminate only this trial's testbench-owned daemon. Normal
	# shutdown has already removed its pid file. Keep raw diagnostic artifacts.
	if [ -s "$RSYSLOG_PIDBASE.pid" ]; then
		shutdown_immediate
		wait_shutdown
	fi
	rm -f "$PORT_FILE" "$PERF_FILE" "$STRACE_FILE" "$VG_FILE" "$LOCK_FILE" "$PERF_RECORD_FILE"
	rm -f "$BENCH_METRIC_FILE.control" "$BENCH_METRIC_FILE.events"
}
trap cleanup EXIT
# Each fresh response is produced only after its control command. A completed
# or zombie child cannot acknowledge the pre-HUP hold or post-activation write.
sender_command() {
	printf '%s\n' "$1" >&7 || error_exit 1 "sender control channel closed"
	IFS= read -r -t 30 event <&6 || error_exit 1 "sender telemetry EOF/timeout"
	printf '%s\n' "$event" >>"$BENCH_METRIC_FILE.sender.jsonl"
	python3 - "$event" "$sender_token" "$2" "$BENCH_MESSAGES" <<'PY'
import json, sys
event = json.loads(sys.argv[1])
if (event['token'] != sys.argv[2] or event['event'] != sys.argv[3] or
        event['sent'] + event['pending'] != int(sys.argv[4]) or
        not (event['pending'] > 0 if event['event'] != 'completed' else event['pending'] == 0)):
    raise SystemExit('invalid sender command response')
PY
	[ "$?" -eq 0 ] || error_exit 1 "invalid sender telemetry"
}
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
template(name="benchOut" type="string" string="%msg%\n")
ruleset(name="reloadProbe") { set $.reloadControl = 0; stop }
'
if [ "$BENCH_PHASE" = reload ] && [ "$BENCH_ROLE" = candidate ]; then
	add_conf 'global(config.reloadOnHUP="on")'
fi
if [ "$BENCH_RULESET" = named ]; then
	# shellcheck disable=SC2089
  if [ "$BENCH_QUEUE" = direct ]; then queue='queue.type="Direct"'; else queue='queue.type="LinkedList" queue.dequeueBatchSize="'$BENCH_BATCH_SIZE'" queue.workerThreads="1"'; fi
	# shellcheck disable=SC2090
  add_conf 'ruleset(name="bench" '$queue') { set $.reloadControl = 0; if $msg contains "msgnum:" then action(type="omfile" name="benchSink" file="'"$RSYSLOG_OUT_LOG"'" template="benchOut") }'
  input_ruleset='ruleset="bench"'
else
  if [ "$BENCH_QUEUE" = direct ]; then add_conf 'main_queue(queue.type="Direct")'; else add_conf 'main_queue(queue.type="LinkedList" queue.dequeueBatchSize="'$BENCH_BATCH_SIZE'" queue.workerThreads="1")'; fi
  add_conf 'if $msg contains "msgnum:" then action(type="omfile" name="benchSink" file="'"$RSYSLOG_OUT_LOG"'" template="benchOut")'
  input_ruleset=''
fi
add_conf 'input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$PORT_FILE"'" workerthreads="1" '"$input_ruleset"')'
prefix=''
# The controller is the daemon process.  Exactly one optional wrapper observes
# it, so perf/strace/Valgrind/lock results retain an unambiguous attribution.
if [ "$BENCH_PERF_STAT" = 1 ]; then prefix="perf stat --all-user -x, -o '$PERF_FILE' -e task-clock:u,cycles:u,instructions:u,branches:u,branch-misses:u,cache-misses:u,context-switches:u,cpu-migrations:u,page-faults:u --"; fi
if [ "$BENCH_STRACE_SUMMARY" = 1 ]; then prefix="strace -f -c -o '$STRACE_FILE'"; fi
if [ "$BENCH_ALLOCATION_SUMMARY" = 1 ]; then prefix="valgrind --tool=memcheck --log-file='$VG_FILE'"; fi
if [ "$BENCH_LOCK_REPORT" = 1 ]; then prefix="perf lock record -o '$LOCK_FILE.data' --"; fi
if [ "$BENCH_PERF_RECORD" = 1 ]; then prefix="perf record --no-buildid-cache --buildid-all -g -e cycles:u -o '$PERF_RECORD_FILE' --"; fi
# shellcheck disable=SC2034 # diag.sh consumes this dynamic controller wrapper.
if [ -n "$prefix" ]; then valgrind="$prefix"; fi
# Keep controller diagnostics separate from sender output and retain them as a
# raw artifact. Raw imdiag status, not diagnostic wording, proves activation.
# shellcheck disable=SC2034 # diag.sh consumes this dynamic controller redirection.
if [ "$BENCH_PHASE" = reload ]; then RS_REDIR="> $RELOAD_LOG 2>&1"; fi
if [ -n "${BENCH_CONFIG_ARTIFACT:-}" ]; then cp "$CONF_FILE" "${BENCH_CONFIG_ARTIFACT}.before.conf"; fi
if [ "$BENCH_DISASSEMBLY" = 1 ]; then
	binary=../tools/.libs/rsyslogd
	[ -x "$binary" ] || binary=../tools/rsyslogd
	objdump -d -s -j .text -j .note.gnu.build-id "$binary" >"$DISASSEMBLY_FILE" 2>&1 || \
		error_exit 1 "daemon disassembly failed"
fi
startup
assign_file_content INPUT_PORT "$PORT_FILE"
start_ns=$(date +%s%N)
reload_ns=0
socket_continuity=false
reload_evidence='{}'
if [ "$BENCH_PHASE" = reload ]; then
	# Hold unfinished producer work on the original sockets during activation.
	# This intentionally does NOT measure continuously sending through that hold.
	sender_token="$RSYSLOG_DYNNAME-$BENCH_ARTIFACT_ID"
	mkfifo "$BENCH_METRIC_FILE.control" "$BENCH_METRIC_FILE.events"
	python3 "$(dirname "${BASH_SOURCE[0]}")/runner.py" --sender "$INPUT_PORT" "$BENCH_MESSAGES" \
		"$BENCH_TCP_SESSIONS" "$BENCH_PAYLOAD_BYTES" "$sender_token" \
		<"$BENCH_METRIC_FILE.control" >"$BENCH_METRIC_FILE.events" &
	sender_pid=$!
	# Read/write opens never block waiting for an absent child; response reads
	# have the explicit deadline. A dead/zombie child cannot manufacture a reply.
	exec 7<>"$BENCH_METRIC_FILE.control"
	exec 6<>"$BENCH_METRIC_FILE.events"
	sender_command prepare prepared
	wait_file_lines "$RSYSLOG_OUT_LOG" 1 30
	status_error=0
	status_before=$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || status_error=$?
	printf '%s\n' "$status_before" >"$BENCH_METRIC_FILE.status-before"
	[ "$BENCH_ROLE" = baseline ] || [ "$status_error" -eq 0 ] || error_exit 1 "candidate status unavailable"
	rss_before=$(ps -o rss= -p "$(cat "$RSYSLOG_PIDBASE.pid")" | tr -d ' ')
	# Mutate an EXISTING ruleset only. Default traffic deliberately uses an
	# unused named probe; named traffic changes its own stable-output bench root.
	if [ "$BENCH_RULESET" = named ]; then target=bench; else target=reloadProbe; fi
	sed -i '/ruleset(name="'"$target"'"/s/set \$.reloadControl = 0/set $.reloadControl = 1/' "$CONF_FILE"
	if [ -n "${BENCH_CONFIG_ARTIFACT:-}" ]; then cp "$CONF_FILE" "${BENCH_CONFIG_ARTIFACT}.candidate.conf"; fi
	sender_command hold_before_hup held_before_hup
	reload_start=$(date +%s%N)
  issue_HUP
	status_error=0
	status_after=$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || status_error=$?
	printf '%s\n' "$status_after" >"$BENCH_METRIC_FILE.status-after"
	[ "$BENCH_ROLE" = baseline ] || [ "$status_error" -eq 0 ] || error_exit 1 "candidate status unavailable"
	reload_ns=$(( $(date +%s%N) - reload_start ))
	sender_command after_activation progress_after_activation
	# Persist raw replies before validation so failures retain their cause.
	reload_evidence=$(python3 - "${BASH_SOURCE[0]}" "$BENCH_ROLE" "$status_before" "$status_after" \
		"$BENCH_METRIC_FILE.sender.jsonl" "$BENCH_MESSAGES" "$BENCH_TCP_SESSIONS" <<'PY'
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]).resolve().parent))
from runner import activation_evidence, sender_overlap
_, _, role, before, after, telemetry, messages, sessions = sys.argv
events = [json.loads(line) for line in Path(telemetry).read_text().splitlines()]
incomplete = sender_overlap(events, int(messages), int(sessions))
evidence = {"role": role, "status_before": before, "status_after": after,
            "progress_before": events[1]['sent'], "progress_after": events[2]['sent'],
            "sender_events": events, "sender_token": events[0]['token'],
            "sender_incomplete_before_and_after": incomplete,
            "boundary": "activated" if role == "candidate" else "legacy_hup"}
if not incomplete:
    raise SystemExit("reload lacks established sender progress on both sides")
if role == "candidate" and not activation_evidence(before, after):
    raise SystemExit("candidate did not prove ON generation 1-to-2 activation")
print(json.dumps(evidence))
PY
	) || error_exit 1 "reload activation oracle failed; raw status artifacts retained"
	rss_after=$(ps -o rss= -p "$(cat "$RSYSLOG_PIDBASE.pid")" | tr -d ' ')
	sender_command finish completed
	wait "$sender_pid"
	sender_pid=''
	exec 7>&-
	exec 6<&-
	# The controlled sender reuses its original sockets for all phases.
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
# optional collectors supplement the separate required acceptance manifests.
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
python3 - "${BASH_SOURCE[0]}" "$PERF_FILE" "$STRACE_FILE" "$VG_FILE" "$LOCK_FILE" "$BENCH_METRIC_FILE" <<'PY'
import json, re, sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]).resolve().parent))
from acceptance import user_stat_values
perf, strace, vg, locks, out = sys.argv[2:]
def read(path):
    try: return open(path, encoding='utf-8', errors='replace').read()
    except OSError: return ''
def syscall_values(text):
    values = {}
    for line in text.splitlines():
        fields=line.split()
        if len(fields) in (5,6) and fields[0][:1].isdigit() and fields[-1] != 'total':
            try: values[fields[-1]]={'calls':int(fields[3]), 'errors':int(fields[4]) if len(fields)==6 else 0}
            except ValueError: pass
    return values
heap = re.search(r'total heap usage: ([0-9,]+) allocs, ([0-9,]+) frees', read(vg))
data={'perf_stat': user_stat_values(read(perf)), 'perf_measurement_scope': 'user',
      'syscalls': syscall_values(read(strace)),
      'allocations': int(heap.group(1).replace(',', '')) if heap else None,
      'lock_report_available': bool(read(locks))}
open(out + '.instrumentation', 'w').write(json.dumps(data))
PY
instrumentation=$(cat "$BENCH_METRIC_FILE.instrumentation")
printf '{"workload_id":"%s","payload_bytes":%d,"tcp_sessions":%d,"ruleset":"%s","queue":"%s","batch_size":%d,"phase":"%s","messages":%d,"execution":"completed","throughput_messages_per_second":%.3f,"cpu_seconds_per_message":null,"send_ns":%d,"delivery_latency_ns":%d,"reload_ns":%d,"rss_before_kib":%d,"rss_after_kib":%d,"hot_path_invariants":{"exact_delivery":true,"delivery_oracle":"%s","hup_processed":%s,"socket_continuity":%s},"reload_evidence":%s,"instrumentation":%s}\n' \
 "$BENCH_WORKLOAD_ID" "$BENCH_PAYLOAD_BYTES" "$BENCH_TCP_SESSIONS" "$BENCH_RULESET" "$BENCH_QUEUE" "$BENCH_BATCH_SIZE" "$BENCH_PHASE" "$BENCH_MESSAGES" \
 "$(awk -v n="$BENCH_MESSAGES" -v t="$((delivered_ns-start_ns))" 'BEGIN{print n*1000000000/t}')" "$((sent_ns-start_ns))" "$((delivered_ns-sent_ns))" "$reload_ns" "$rss_before" "$rss_after" \
 "$oracle" "$( [ "$BENCH_PHASE" = reload ] && echo true || echo false )" "$socket_continuity" "$reload_evidence" "$instrumentation" >"$BENCH_METRIC_FILE"
exit_test
