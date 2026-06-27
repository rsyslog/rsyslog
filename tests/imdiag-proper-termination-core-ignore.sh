#!/usr/bin/env bash
# Verify regular diag.sh-generated rsyslog tests get a proper termination
# marker automatically. A clean shutdown with that marker must not fail because
# a generic core.* file from another parallel test is present in the test dir.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=1

test_error_exit_handler() {
	rm -f core.foreign-from-other-test
}

generate_conf
add_conf 'action(type="omfile" file="'$RSYSLOG_OUT_LOG'")'
startup
injectmsg 0 "$NUMMESSAGES"
touch core.foreign-from-other-test
shutdown_when_empty
wait_shutdown

rm -f core.foreign-from-other-test

exit_test
