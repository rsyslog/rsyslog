#!/bin/bash
# added 2026-05-17 by Codex, released under ASL 2.0
# Sends invalid bytes to a zlib stream:always imtcp listener; the oracle is the
# invalid-stream log message and no submitted syslog payload.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
check_command_available python3
wait_invalid_stream_log() {
	content_check --check-only "imtcp: received invalid compressed stream" "$RSYSLOG_OUT_LOG"
}
export QUEUE_EMPTY_CHECK_FUNC=wait_invalid_stream_log

generate_conf
add_conf '
$MaxMessageSize 10k
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zlib")

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
sock.sendall(b"this-is-not-a-valid-zlib-stream")
sock.close()
PY

shutdown_when_empty
wait_shutdown

content_check "imtcp: received invalid compressed stream"
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: corrupt compressed input unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
