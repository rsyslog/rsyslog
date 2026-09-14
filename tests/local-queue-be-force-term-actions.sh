#!/bin/bash
# Verify cooperative interrupted transaction ownership for a BE-origin batch.
# imdiag injects one message with no trusted input producer, and the final
# snapshot must prove zero FE registrations. Actual Direct omfile A commits 1;
# the existing test-only dispatch gate suspends B only after BE immediate
# shutdown is observable. The real actionCommit returns FORCE_TERM and resets
# currIParam. Source completion must retain that message, but the sole BE
# worker is stopping, so there is no replay consumer. Joined shutdown must
# explicitly discard that obligation: A remains 1 and B stays empty. After the
# blocked snapshot, every additional admission is also shutdown-discarded;
# none may become a terminal success. This includes the daemon's own shutdown
# diagnostic without hard-coding its presence. The existing final snapshot
# verifies full lifetime conservation. Terminal alone is not a delivery oracle.
# The 10s action deadline is only a watchdog; marker acknowledgements control
# release, and no elapsed-time threshold determines success.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imdiag
require_plugin impstats

COMMIT_ENTRY="$PWD/${RSYSLOG_DYNNAME}.be-transaction-entry"
COMMIT_RELEASE="$PWD/${RSYSLOG_DYNNAME}.be-transaction-release"
STOP_BASE="$PWD/${RSYSLOG_DYNNAME}.be-transaction-stop"
ACTION_A="$PWD/${RSYSLOG_DYNNAME}.be-transaction-a"
ACTION_B="$PWD/${RSYSLOG_DYNNAME}.be-transaction-b"
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.be-transaction-stats"
mkfifo "$COMMIT_RELEASE"
export RSYSLOG_LOCAL_QUEUE_TEST_COMMIT_ACTION=localq-be-b
export RSYSLOG_LOCAL_QUEUE_TEST_COMMIT_ENTRY="$COMMIT_ENTRY"
export RSYSLOG_LOCAL_QUEUE_TEST_COMMIT_RELEASE="$COMMIT_RELEASE"

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.timeoutShutdown="1" queue.timeoutActionCompletion="10000"
	queue.local.frontendSize="4" queue.local.maxFrontends="1")
template(name="localqvalue" type="string" string="%$!localq!attempt%\n")
if ($msg contains "msgnum:00000000:") then {
	set $!localq!attempt = $!localq!attempt + 1;
	action(name="localq-be-a" type="omfile" file="'$ACTION_A'" template="localqvalue" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on" action.reportSuspension="off"
		action.reportSuspensionContinuation="off")
	action(name="localq-be-b" type="omfile" file="'$ACTION_B'" template="localqvalue" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on" action.reportSuspension="off"
		action.reportSuspensionContinuation="off")
}
'
startup

# Start the sole BE worker without entering the transaction gate, then flush
# its startup internal messages. The standard imdiag empty-queue handshake is
# valid here because this fixture never creates FE producers or holdings.
# Otherwise those startup messages could also remain behind the blocked B.
injectmsg 1 1
wait_queueempty

response=$(printf "localqueuestopcheck %s\n" "$STOP_BASE" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: local queue BE stop-check arm response: $response"
	error_exit 1
fi

injectmsg 0 1
wait_file_lines "$COMMIT_ENTRY" 1
localq_wait_stats "$STATSFILE" "main Q.local" "fe.registered=0" "be.active.messages=1" \
	"outstanding.messages=1" "terminal.shutdown_discarded.messages=0"
before_terminal=$(grep -F "main Q.local: origin=core.queue.local " "$STATSFILE" | tail -n 1 | \
	sed -n 's/.* terminal.messages=\([0-9][0-9]*\).*/\1/p')
case "$before_terminal" in
	''|*[!0-9]*) error_exit 1 'missing blocked BE terminal baseline' ;;
esac
shutdown_immediate
wait_file_lines "${STOP_BASE}.action-phase" 1
localq_release_barrier "$COMMIT_RELEASE"
wait_file_lines "${STOP_BASE}.force-term" 1
wait_shutdown
wait_file_lines "$STOP_BASE" 1

# cmp_exact reads EXPECTED dynamically from diag.sh.
# shellcheck disable=SC2034
EXPECTED='1'
cmp_exact "$ACTION_A"
if [ -s "$ACTION_B" ]; then
	echo 'FAIL: stopped BE transaction unexpectedly invoked or replayed B'
	cat "$ACTION_B"
	error_exit 1
fi
if ! grep -Eq '^OK fe.joined=0 fe.registered=0 shutdown.discarded=[1-9][0-9]* outstanding=0 ' "$STOP_BASE"; then
	echo 'FAIL: interrupted BE obligation was not explicitly shutdown-discarded'
	cat "$STOP_BASE"
	error_exit 1
fi
final_admitted=$(sed -n 's/.* admitted=\([0-9][0-9]*\).*/\1/p' "$STOP_BASE")
final_discarded=$(sed -n 's/.* shutdown.discarded=\([0-9][0-9]*\).*/\1/p' "$STOP_BASE")
case "$final_admitted:$final_discarded" in
	*[!0-9:]*|:*|*:) error_exit 1 'missing final BE disposition counters' ;;
esac
if [ "$final_discarded" -ne "$((final_admitted - before_terminal))" ]; then
	echo 'FAIL: stopped BE counted an unresolved obligation as terminal success'
	cat "$STOP_BASE"
	error_exit 1
fi
exit_test
