#!/bin/bash
# Run the testbench-only queue source-attribution completion fixture through
# imdiag in an initialized local-queue daemon. Its explicit OK reply proves
# both distinct callback-owner/source terminal paths and a real ConsumerReg()
# empty-BE acquisition using a fresh worker whose shutdown pointer is NULL.
# The fixture invokes that consumer with both shutdown flag values while it
# holds the BE mutex, so the empty/first-worker regression is deterministic
# rather than a worker-scheduling race. Clean shutdown proves the fixture
# restored the real queue state.

. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"

generate_conf
localq_make_startup_marker_absolute
add_conf '
$AbortOnUncleanConfig on
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1"
	queue.dequeueBatchSize="1" queue.local.frontendSize="4" queue.local.maxFrontends="1")
'
startup

response=$(printf 'waitmainqueueempty\n' | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"mainqueue empty"* ]]; then
	echo "FAIL: local BE did not quiesce before queue lease fixture: $response"
	error_exit 1
fi
response=$(printf 'queueleasetest\n' | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: queue source-attribution fixture response: $response"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
