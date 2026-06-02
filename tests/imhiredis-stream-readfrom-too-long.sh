#!/usr/bin/env bash
# Verify that imhiredis rejects stream.readFrom values that exceed the fixed index buffer.
. ${srcdir:=.}/diag.sh init

LONG_STREAM_INDEX=$(printf '1%.0s' $(seq 1 44))
modpath="../runtime/.libs:../contrib/imhiredis/.libs:../.libs"

generate_conf
add_conf '
module(load="../contrib/imhiredis/.libs/imhiredis")
input(type="imhiredis"
      key="mystream"
      mode="stream"
      stream.readFrom="'$LONG_STREAM_INDEX'")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$modpath" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 1 ]; then
	echo "FAIL: expected config validation failure for oversize imhiredis stream.readFrom"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

grep -F "configured stream.readFrom value exceeds maximum supported stream index length" \
	"${RSYSLOG_DYNNAME}.log" >/dev/null || {
	echo "FAIL: expected imhiredis stream.readFrom validation error"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
}

exit_test
