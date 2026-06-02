#!/bin/bash
# Verify that imczmq rejects configured topics that exceed the supported limit.
. ${srcdir:=.}/diag.sh init

LONG_TOPIC=$(printf 'a%.0s' $(seq 1 256))
modpath="../runtime/.libs:../contrib/imczmq/.libs:../.libs"

generate_conf
add_conf '
module(load="../contrib/imczmq/.libs/imczmq")
input(type="imczmq"
      endpoints="inproc://imczmq-topic-too-long"
      socktype="SUB"
      topics="'$LONG_TOPIC'")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$modpath" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 1 ]; then
	echo "FAIL: expected config validation failure for oversize imczmq topic"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

grep -F "configured topic exceeds maximum supported length" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
	echo "FAIL: expected imczmq oversize topic validation error"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
}

exit_test
