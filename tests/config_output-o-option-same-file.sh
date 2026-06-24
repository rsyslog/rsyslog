#!/bin/bash
# Verify that -o refuses to write to the same file that -f is reading.
# The oracle is twofold: rsyslogd must reject the unsafe configuration before
# opening the output path, and the input config must remain byte-for-byte
# unchanged. This covers the direct same-path case and a symlink alias.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

cp "${TESTCONF_NM}.conf" "$RSYSLOG_DYNNAME.original.conf"
stderr="$RSYSLOG_DYNNAME.stderr"

if ../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -o"${TESTCONF_NM}.conf" \
	-M../runtime/.libs:../.libs >"$RSYSLOG_DYNNAME.stdout" 2>"$stderr"; then
	echo "FAIL: rsyslogd accepted -o pointing at its input config"
	error_exit 1
fi
content_check "-o output path must not be the same as the input config file" "$stderr"

if ! cmp -s "$RSYSLOG_DYNNAME.original.conf" "${TESTCONF_NM}.conf"; then
	echo "FAIL: rsyslogd modified the input config in the direct same-path case"
	echo "original:"
	cat -n "$RSYSLOG_DYNNAME.original.conf"
	echo "current:"
	cat -n "${TESTCONF_NM}.conf"
	error_exit 1
fi

ln -sf "$(basename "${TESTCONF_NM}.conf")" "$RSYSLOG_DYNNAME.same-link.conf"
if ../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -o"$RSYSLOG_DYNNAME.same-link.conf" \
	-M../runtime/.libs:../.libs >"$RSYSLOG_DYNNAME.link.stdout" 2>"$stderr"; then
	echo "FAIL: rsyslogd accepted -o pointing at a symlink to its input config"
	error_exit 1
fi
content_check "-o output path '$RSYSLOG_DYNNAME.same-link.conf' resolves to the same file" "$stderr"

if ! cmp -s "$RSYSLOG_DYNNAME.original.conf" "${TESTCONF_NM}.conf"; then
	echo "FAIL: rsyslogd modified the input config through a symlink output path"
	echo "original:"
	cat -n "$RSYSLOG_DYNNAME.original.conf"
	echo "current:"
	cat -n "${TESTCONF_NM}.conf"
	error_exit 1
fi

exit_test
