#!/bin/bash
# Run the testbench-only queue source-attribution completion fixture through
# imdiag in an initialized daemon. The explicit OK reply proves the fixture
# completed both distinct callback-owner/source terminal paths; clean shutdown
# proves it left the configured queue usable. It does not model cancellation,
# live message refcounts, or segmented-store internals.

. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '$AbortOnUncleanConfig on'
startup

response=$(printf 'queueleasetest\n' | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: queue source-attribution fixture response: $response"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
