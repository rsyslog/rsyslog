#!/bin/bash
# Verify local stats adapter destruction serializes with the actual native
# statsobj reader in an initialized local-queue daemon. The testbench-only
# imdiag command blocks the adapter's pre-read callback under statsobj's global
# list mutex, starts qqueueLocalStatsDestruct concurrently, and releases it
# only after observing destruction start. Its OK reply proves the reader and
# destructor joined and a second native walk found no core.queue.local objects;
# clean shutdown proves the queue tolerates its already-unlinked adapter.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imdiag

generate_conf
localq_make_startup_marker_absolute
add_conf '
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="16"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1"
	queue.local.frontendSize="4" queue.local.maxFrontends="2" queue.local.frontendStats="on")
'
startup

response=$(printf "localqueuestatslifetimetest\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: local queue stats lifetime fixture response: $response"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
