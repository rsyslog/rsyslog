#!/bin/bash
# added 2026-05-19 by Codex, released under ASL 2.0
# Opens and closes a zstd stream:always listener without sending bytes; the
# oracle is the absence of a misleading truncated-stream diagnostic.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime
check_command_available python3

generate_conf
add_conf '
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zstd")

if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
	stop
}
action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

python3 - "$TCPFLOOD_PORT" <<'PY'
import socket
import sys

port = int(sys.argv[1])
sock = socket.create_connection(("127.0.0.1", port))
sock.close()
PY

shutdown_when_empty
wait_shutdown

if [ -f "$RSYSLOG_OUT_LOG" ] && grep -q "detected truncated compressed stream" "$RSYSLOG_OUT_LOG"; then
	echo "FAIL: empty zstd connection logged a truncated compressed stream"
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: empty zstd connection unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
