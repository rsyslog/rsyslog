#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that include(mode="abort-if-missing") remains an unconditional hard
# configuration error without terminating from inside the parser. The exact
# exit status, missing-include diagnostic and post-Load finalization diagnostic
# prove ordinary init unwinding, not an exit(1) from parser code, even though
# abortOnUncleanConfig defaults to off. Process output is intentional here:
# startup fails before a usable configured output exists. A present-include
# control must complete the normal -N1 validation path successfully.
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
content_check "rsyslogd: run failed with error -2104 (see rsyslog.h" "${RSYSLOG_DYNNAME}.log"

generate_conf
printf '%s\n' 'action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")' >"${RSYSLOG_DYNNAME}.include.conf"
add_conf 'include(file="'"$RSYSLOG_DYNNAME"'.include.conf" mode="abort-if-missing")'
../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.control.log" 2>&1 || error_exit 1
content_check "rsyslogd: End of config validation run. Bye." "${RSYSLOG_DYNNAME}.control.log"

exit_test
