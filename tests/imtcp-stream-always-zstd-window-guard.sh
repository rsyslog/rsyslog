#!/bin/bash
# added 2026-06-23 by Codex, released under ASL 2.0
# Sends a minimal zstd frame that advertises a 128 MiB window while the imtcp
# input allows only a 1 MiB decompressed receive burst. The oracle is that zstd
# rejects the frame as an invalid compressed stream before any message is
# submitted, proving the configured receive limit also bounds decoder windows.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime
check_command_available python3

wait_invalid_stream_log() {
	content_check --check-only "received invalid compressed stream" "$RSYSLOG_OUT_LOG"
}
export QUEUE_EMPTY_CHECK_FUNC=wait_invalid_stream_log

generate_conf
add_conf '
$MaxMessageSize 10k
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zstd"
	compression.maxDecompressedBytesPerReceive="1048576")

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
# Empty zstd frame with a large single-segment/window descriptor. This is the
# compact reproducer used for the pre-output allocation path.
payload = bytes.fromhex("28b52ffd0088010000")
sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(payload)
sock.close()
PY

shutdown_when_empty
wait_shutdown

content_check "received invalid compressed stream" "$RSYSLOG_OUT_LOG"
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: window-guarded zstd stream unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
