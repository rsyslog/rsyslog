#!/bin/bash
# Verify cooperative transactional Direct shutdown retention and replay. One
# message increments its message-local value, then commits named omfile A and
# named omfile B. The test-only action dispatch hook blocks B and later returns
# SUSPENDED; it never substitutes an omfile FORCE_TERM result. After the armed
# local queue exposes its immediate action phase, releasing the FIFO makes the
# real actionCommit retry path return FORCE_TERM and reset currIParam. FE work
# is retained for BE replay: A records values 1 then 2, B records only 2. This
# deliberately proves repeatable script/action effects, not exact-once output.
# The force-term marker observes the real post-reset return and the final marker
# proves whole-lifetime local-queue obligation conservation for injected messages.
# Internal messages are disabled so they do not alter the sampling oracle.
# The S6 wrapper samples three ingress IDs down to ID2;
# its unchanged A/B replay oracle also proves retry never samples again. The 10 s
# action deadline below is only a harness watchdog: phase-marker acknowledgement,
# rather than elapsed time, controls the FIFO release.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag

sampling_config=''
input_count=1
blocked_id=00000000
if [ "${LOCAL_QUEUE_S6_RETRY_SAMPLING:-0}" = 1 ]; then
    sampling_config='queue.samplingInterval="3"'
    input_count=3
    blocked_id=00000002
fi
COMMIT_ENTRY="$PWD/${RSYSLOG_DYNNAME}.transaction-entry"
COMMIT_RELEASE="$PWD/${RSYSLOG_DYNNAME}.transaction-release"
STOP_BASE="$PWD/${RSYSLOG_DYNNAME}.transaction-stop"
ACTION_A="$PWD/${RSYSLOG_DYNNAME}.transaction-a"
ACTION_B="$PWD/${RSYSLOG_DYNNAME}.transaction-b"
mkfifo "$COMMIT_RELEASE"

# The action hook is cold-configured during action activation. The FIFO exists
# before startup, while the test writes its single release only after immediate
# shutdown is observable through STOP_BASE.action-phase.
export RSYSLOG_LOCAL_QUEUE_TEST_COMMIT_ACTION=localq-b
export RSYSLOG_LOCAL_QUEUE_TEST_COMMIT_ENTRY="$COMMIT_ENTRY"
export RSYSLOG_LOCAL_QUEUE_TEST_COMMIT_RELEASE="$COMMIT_RELEASE"

generate_conf
localq_make_startup_marker_absolute
add_conf '
global(processInternalMessages="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1" '"$sampling_config"'
	queue.timeoutShutdown="1" queue.timeoutActionCompletion="10000"
	queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqvalue" type="string" string="%$!localq!attempt%\n")
if ($msg contains "msgnum:'$blocked_id':") then {
	set $!localq!attempt = $!localq!attempt + 1;
	action(name="localq-a" type="omfile" file="'$ACTION_A'" template="localqvalue" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on" action.reportSuspension="off"
		action.reportSuspensionContinuation="off")
	action(name="localq-b" type="omfile" file="'$ACTION_B'" template="localqvalue" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on" action.reportSuspension="off"
		action.reportSuspensionContinuation="off")
}
'
startup

response=$(printf "localqueuestopcheck %s\n" "$STOP_BASE" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: local queue stop-check arm response: $response"
	error_exit 1
fi

tcpflood -m"$input_count" -i0 &
sender=$!
wait_file_lines "$COMMIT_ENTRY" 1
wait "$sender" || error_exit $?

shutdown_immediate
wait_file_lines "${STOP_BASE}.action-phase" 1
localq_release_barrier "$COMMIT_RELEASE"
wait_file_lines "${STOP_BASE}.force-term" 1
wait_file_lines "$ACTION_A" 2
wait_file_lines "$ACTION_B" 1
wait_shutdown
wait_file_lines "$STOP_BASE" 1

# cmp_exact, sourced from diag.sh, reads this intentional global fixture value.
# shellcheck disable=SC2034
EXPECTED=$'1\n2'
cmp_exact "$ACTION_A"
# shellcheck disable=SC2034
EXPECTED='2'
cmp_exact "$ACTION_B"
if ! grep -Eq '^OK .*outstanding=0 ' "$STOP_BASE"; then
	echo "FAIL: final local queue transaction reconciliation marker"
	cat "$STOP_BASE" 2>/dev/null || true
	error_exit 1
fi
exit_test
