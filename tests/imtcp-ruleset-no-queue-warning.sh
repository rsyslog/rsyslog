#!/bin/bash
# Verify issue #1324: an input bound to a ruleset without its own non-direct
# queue emits a config warning, while a ruleset with queue.type="LinkedList"
# stays warning-free. The oracle is rsyslogd -N1 output, so no listener is
# started and no socket readiness timing is involved.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")

ruleset(name="shared") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="shared")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.warn.log" 2>&1
if [ $? -ne 0 ]; then
	echo "FAIL: expected config validation to continue after ruleset queue warning"
	cat "${RSYSLOG_DYNNAME}.warn.log"
	error_exit 1
fi

content_check "input-bound ruleset 'shared' has no dedicated non-direct queue" \
	"${RSYSLOG_DYNNAME}.warn.log"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")

ruleset(name="queued" queue.type="LinkedList") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="queued")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.quiet.log" 2>&1
if [ $? -ne 0 ]; then
	echo "FAIL: expected config validation to accept queued ruleset binding"
	cat "${RSYSLOG_DYNNAME}.quiet.log"
	error_exit 1
fi

check_not_present "input-bound ruleset 'queued' has no dedicated non-direct queue" \
	"${RSYSLOG_DYNNAME}.quiet.log"

exit_test
