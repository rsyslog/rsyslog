#!/bin/bash
# Verify localHostname is applied to internal rsyslogd messages (hostname property).
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
# Make sure local hostname is a FQDN and preserved.
global(preserveFQDN="on" localHostname="host.example.com")

template(name="outfmt" type="string" string="%hostname%\n")

if ($syslogtag contains "rsyslogd") then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	stop
}'

startup
./msleep 100
shutdown_when_empty
wait_shutdown

line_count=$(wc -l < "$RSYSLOG_OUT_LOG")
if [ "$line_count" -eq 0 ]; then
	printf 'expected hostname output, got empty log\n'
	error_exit 1
fi
content_count_check "host.example.com" "$line_count"

exit_test
