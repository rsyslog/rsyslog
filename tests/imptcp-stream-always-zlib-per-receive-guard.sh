#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-09, released under ASL 2.0
# Regression test for the imptcp per-receive decompressed-byte cap.
# Stimulus: the same ~270:1 zlib stream (a few hundred compressed bytes expanding
# to ~297 KB) sent in one write to a listener with the ratio guard disabled
# (compression.maxExpansionRatio="0") and
# compression.maxDecompressedBytesPerReceive="1000".
# Oracle: imptcp logs the per-receive limit, and no decoded message reaches the
# output file (the first inflated chunk already exceeds the cap).
. ${srcdir:=.}/diag.sh init
require_plugin imptcp
check_command_available python3
wait_invalid_stream_log() {
	content_check --check-only "decompressed bytes exceeded configured per-receive limit" "$RSYSLOG_OUT_LOG"
}
export QUEUE_EMPTY_CHECK_FUNC=wait_invalid_stream_log

generate_conf
add_conf '
$MaxMessageSize 10k
global(processInternalMessages="on")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.maxExpansionRatio="0"
	compression.maxDecompressedBytesPerReceive="1000"
)

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
import zlib

port = int(sys.argv[1])
line = b"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
payload = line * 3000
compressed = zlib.compress(payload, level=9)
sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(compressed)
sock.close()
PY

shutdown_when_empty
wait_shutdown

content_check "decompressed bytes exceeded configured per-receive limit"
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: guarded zlib stream unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
