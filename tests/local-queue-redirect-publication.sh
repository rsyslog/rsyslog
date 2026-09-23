#!/bin/bash
# Force a publisher to pause after it marks publishing and before it rechecks
# LOCAL_RUNNING. The testbench-only gate then lets qqueueLocalShutdown set
# REDIRECT, so the real qqueueLocalSubmit() call must send its one reference to
# BE. The initialized-daemon fixture checks either one terminal BE outcome or
# one classified pre-admission rejection after backend closure, never an FE
# publication, and the redirect counter before it replies. Its explicit
# producer/shutdown handoff replaces scheduling sleeps; normal shutdown remains
# the daemon termination oracle.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imdiag

generate_conf
localq_make_startup_marker_absolute
add_conf '
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="16"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.timeoutShutdown="1" queue.timeoutActionCompletion="1"
	queue.local.frontendSize="4" queue.local.maxFrontends="1")
template(name="localqfmt" type="string" string="%msg%\n")
action(type="omfile" file="/dev/null" template="localqfmt" queue.type="Direct"
	asyncWriting="off" flushOnTXEnd="on")
'
startup

response=$(printf "localqueueredirecttest\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in
    *"OK redirect.messages=1 outcome="*) ;;
    *) echo "FAIL: unexpected redirect fixture response: $response"; error_exit 1 ;;
esac

shutdown_when_empty
wait_shutdown
exit_test
