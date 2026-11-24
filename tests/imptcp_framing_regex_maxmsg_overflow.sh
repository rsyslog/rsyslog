#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject regex framing when maxMessageSize exceeds the signed doubling limit.
# Startup must reject the config with the specific diagnostic before a listener starts;
# Exit status must be nonzero and not timeout (124); process stderr supplies
# the diagnostic oracle because activation cannot route through omfile.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(maxMessageSize="1073741824" inputStartupPolicy="require-all")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" framing.delimiter.regex="^<[0-9]{2}>")
'

timeout 10s ../tools/rsyslogd -n -iNONE -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.log" 2>&1
status=$?
if [ "$status" -eq 124 ]; then
    error_exit 1 "imptcp regex size validation timed out during startup"
fi
if [ "$status" -eq 0 ]; then
    echo "FAIL: expected imptcp regex size validation failure"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi
content_check 'imptcp: framing.delimiter.regex requires maxMessageSize <=' "${RSYSLOG_DYNNAME}.log"

exit_test
