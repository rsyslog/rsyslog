#!/bin/bash
# Validate that an excessively large imbeats maxSessions value is not rejected
# during config load. The oracle is rsyslogd -N1 succeeding while emitting the
# explicit cap warning, so the test avoids opening listeners or sending traffic.
. ${srcdir:=.}/diag.sh init
require_plugin imbeats

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

input(type="imbeats"
      port="0"
      maxSessions="4294967296")
'

if ! ../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.log" 2>&1; then
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

content_check 'imbeats: maxSessions value 4294967296 exceeds UINT_MAX' "${RSYSLOG_DYNNAME}.log"
content_check 'capped at UINT_MAX' "${RSYSLOG_DYNNAME}.log"

exit_test
