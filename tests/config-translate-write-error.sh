#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that translation reports a buffered output write failure.
# The oracle is a nonzero rsyslogd exit for /dev/full and a successful,
# nonempty regular output file for the same valid input configuration.
. ${srcdir:=.}/diag.sh init

if [ ! -e /dev/full ]; then
	echo "SKIP: /dev/full is unavailable"
	exit 77
fi

modpath="../runtime/.libs:../.libs"
cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
action(type="omfile" file="/tmp/rsyslog-translate-write-error.log")
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml \
	-o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" || error_exit $?
if [ ! -s "${RSYSLOG_DYNNAME}.yaml" ]; then
	echo "FAIL: translation did not produce the expected regular file"
	error_exit 1
fi

if ../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml \
	-o /dev/full -M"$modpath" >"${RSYSLOG_DYNNAME}.stdout" \
	2>"${RSYSLOG_DYNNAME}.stderr"; then
	echo "FAIL: translation accepted a failed output write"
	error_exit 1
fi

echo SUCCESS: translation reports output write errors
exit_test
