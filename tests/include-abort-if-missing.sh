#!/bin/bash
# Verify that include(mode="abort-if-missing") remains an unconditional hard
# configuration error without terminating from inside the parser. The exact
# exit status and diagnostic prove that the error propagated through Load and
# normal init cleanup even though abortOnUncleanConfig defaults to off.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf 'include(file="'"$RSYSLOG_DYNNAME"'.does-not-exist" mode="abort-if-missing")'

set +e
../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
rc=$?
set -e
if [ "$rc" -ne 1 ]; then
	echo "FAIL: abort-if-missing returned $rc instead of 1"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi
content_check "is missing and mode is abort-if-missing" "${RSYSLOG_DYNNAME}.log"

exit_test
