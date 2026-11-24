#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Hold one generic global ruleset worker in a FIFO action, then queue exactly
# two more messages behind it. Impstats reports size 3 while the first message
# is in flight and the other two are queued. The post-exit operatingStateFile
# must report local=0/global=3 exactly once. This proves the graph finalizer
# counts and drains every residual without routing its report back through the
# stopped graph.
# The FIFO marker controls the active callback; polling the exact queue size is
# the backlog oracle, and the test timeout only bounds a failed shutdown.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imdiag
require_plugin impstats
require_plugin omtesting
global_queue_type=${LOCAL_QUEUE_GLOBAL_QUEUE_TYPE:-FixedArray}

STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
OSFFILE="$PWD/$RSYSLOG_DYNNAME.osf"
MIDDLE_ENTER="$PWD/$RSYSLOG_DYNNAME.middle-enter"
MIDDLE_RELEASE="$PWD/$RSYSLOG_DYNNAME.middle-release"
mkfifo "$MIDDLE_RELEASE"
# Keep the FIFO open so the action blocks in read(), where shutdown can cancel
# it after its zero-length action deadline.
exec {middle_release_fd}<>"$MIDDLE_RELEASE"
# error_exit releases the held action before shutting down the daemon, so a
# failed assertion cannot leave the worker blocked on the FIFO.
test_error_exit_handler() {
	printf 'release\n' >&"$middle_release_fd" || true
	exec {middle_release_fd}>&-
}

generate_conf
localq_make_startup_marker_absolute
add_conf '
global(processInternalMessages="off" operatingStateFile="'$OSFFILE'" abortOnUncleanConfig="on")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/omtesting/.libs/omtesting")
template(name="middlefmt" type="string" string="%msg%\n")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="0" queue.timeoutActionCompletion="0"
    queue.local.frontendSize="4" queue.local.maxFrontends="1")
ruleset(name="middle" queue.scope="global" queue.type="'$global_queue_type'" queue.size="16"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="0" queue.timeoutActionCompletion="0") {
    if ($msg contains "msgnum:00000000:") then
        :omtesting:file_barrier '$MIDDLE_ENTER' '$MIDDLE_RELEASE';middlefmt
}
if ($msg contains "msgnum:") then call middle
'
startup

injectmsg 0 1
wait_file_lines "$MIDDLE_ENTER" 1
injectmsg 1 2
deadline=$((SECONDS + TB_TEST_TIMEOUT))
while :; do
	queue_line=$(grep -F 'middle: origin=core.queue ' "$STATSFILE" 2>/dev/null | tail -n 1 || true)
	if printf '%s\n' "$queue_line" | grep -Eq '(^| )size=3( |$)'; then
		printf 'middle global queue reached exact in-flight-plus-queued size 3: %s\n' "$queue_line"
		break
	fi
	if (( SECONDS >= deadline )); then
		printf 'FAIL: middle global queue did not reach size 3; last record: %s\n' "$queue_line"
		cat "$STATSFILE" 2>/dev/null || true
		error_exit 1
	fi
	$TESTTOOL_DIR/msleep 100
done

shutdown_immediate
wait_shutdown
exec {middle_release_fd}>&-
unset -f test_error_exit_handler
expected_report='shutdown queue loss: local.shutdown_discarded=0 global.memory_queue_residual=3'
if [ "$(grep -Fc "$expected_report" "$OSFFILE")" -ne 1 ]; then
	error_exit 1 'operating state file did not report exactly three generic global memory residuals'
fi
exit_test
