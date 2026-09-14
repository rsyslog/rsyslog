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
# proves the one accepted obligation becomes one terminal obligation.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag

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
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.timeoutShutdown="1" queue.timeoutActionCompletion="1"
	queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqvalue" type="string" string="%$!localq!attempt%\n")
if ($msg contains "localq-force") then {
	set $!localq!attempt = $!localq!attempt + 1;
	action(name="localq-a" type="omfile" file="'$ACTION_A'" template="localqvalue" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
	action(name="localq-b" type="omfile" file="'$ACTION_B'" template="localqvalue" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
}
'
startup

response=$(printf "localqueuestopcheck %s\n" "$STOP_BASE" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: local queue stop-check arm response: $response"
	error_exit 1
fi

tcpflood -m1 -M'localq-force' &
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

EXPECTED=$'1\n2'
cmp_exact "$ACTION_A"
EXPECTED='2'
cmp_exact "$ACTION_B"
if ! grep -Eq '^OK .*outstanding=0 admitted=1 terminal=1 rejected=0$' "$STOP_BASE"; then
	echo "FAIL: final local queue transaction reconciliation marker"
	cat "$STOP_BASE" 2>/dev/null || true
	error_exit 1
fi
exit_test
