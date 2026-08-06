#!/bin/bash
# Reproduce concurrent access to an mmnormalize Turbo worker context while HUP
# replaces that context. A large TCP flood keeps all main-queue workers inside
# normalization while acknowledged HUPs repeatedly rebuild their private
# liblognorm contexts. Under ThreadSanitizer, an unsafe implementation is
# expected to report the ctxlnTurbo race and abort rsyslogd. The post-fix oracle
# is that every sender completes, every message reaches the output, and rsyslogd
# shuts down normally without a sanitizer report. The high message count keeps
# work pending throughout the HUP loop; the HUP count supplies many independent
# overlap opportunities without relying on a timing sleep. The 15-minute outer
# timeout is only a deadlock guard for heavily instrumented CI hosts; normal
# reproductions finish or raise the sanitizer report within seconds.
export TEST_MAX_RUNTIME=900
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

export NUMMESSAGES="${NUMMESSAGES:-500000}"
HUP_COUNT="${HUP_COUNT:-50}"
TSAN_LOG="$PWD/$RSYSLOG_DYNNAME.tsan"
export TSAN_OPTIONS="${TSAN_OPTIONS:+$TSAN_OPTIONS:}log_path=$TSAN_LOG"
# Clang 21's live symbolizer can block while resolving this dlopen()ed module,
# hiding an already-detected race. Raw module offsets remain deterministic and
# can be resolved offline with llvm-symbolizer or addr2line.
if [[ "$TSAN_OPTIONS" != *"symbolize="* ]]; then
	export TSAN_OPTIONS="$TSAN_OPTIONS:symbolize=0"
fi

printf '%s\n' 'rule=:%payload:rest%' > "$RSYSLOG_DYNNAME.rulebase"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

main_queue(
	queue.type="FixedArray"
	queue.size="600000"
	queue.workerThreads="16"
	queue.workerThreadMinimumMessages="1"
	queue.dequeueBatchSize="1"
)

input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="normalize")

template(name="sequence" type="string" string="%msg:F,58:2%\n")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rulebase="'$RSYSLOG_DYNNAME'.rulebase")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="sequence"
		flushOnTXEnd="on")
}
'

dump_tsan_report() {
	if [ -n "${TSAN_REPORT:-}" ] && [ -s "$TSAN_REPORT" ]; then
		printf 'ThreadSanitizer report from rsyslogd:\n'
		cat "$TSAN_REPORT"
	fi
}

startup
TSAN_REPORT="$TSAN_LOG.$(cat "$RSYSLOG_PIDBASE.pid")"
trap dump_tsan_report EXIT
start_tcpflood_async TCPFLOOD_PID TCPFLOOD_MARKER \
	-c32 -m"$NUMMESSAGES"

# Do not issue the first HUP until normalization is demonstrably active and
# worker-local Turbo contexts have been created.
wait_file_lines "$RSYSLOG_OUT_LOG" 100 60

for ((i = 1; i <= HUP_COUNT; ++i)); do
	printf 'issuing mmnormalize Turbo stress HUP %d/%d\n' "$i" "$HUP_COUNT"
	issue_HUP
	if [ -s "$TSAN_REPORT" ]; then
		printf 'ThreadSanitizer report detected after HUP %d\n' "$i"
		kill "$TCPFLOOD_PID" 2>/dev/null || true
		error_exit 1
	fi
done

wait_tcpflood_async "$TCPFLOOD_PID" "$TCPFLOOD_MARKER"
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1))
exit_test
