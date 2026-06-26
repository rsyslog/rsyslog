#!/bin/bash
# Verify that a bare imuxsock input reports how to configure the intended
# socket. The oracle is rsyslogd -N1 failing with the imuxsock-specific
# diagnostic: additional imuxsock inputs require Socket, while the system log
# socket is configured through module-level SysSock.* parameters.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock")
input(type="imuxsock")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

log="$RSYSLOG_DYNNAME.config-check.log"
if ../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"$log" 2>&1; then
	echo "FAIL: rsyslogd accepted imuxsock input without Socket"
	error_exit 1
fi

content_check 'imuxsock: input(type="imuxsock") defines an additional UNIX socket' "$log"
content_check 'set Socket="..." for that input' "$log"
content_check 'module-level SysSock.* parameters' "$log"

exit_test
