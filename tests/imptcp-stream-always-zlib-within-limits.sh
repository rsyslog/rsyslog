#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-09, released under ASL 2.0
# Positive control for the imptcp decompression guards: a valid ~270:1 zlib
# stream must still be accepted when the configured limits are comfortably above
# its size (maxExpansionRatio="1000", maxDecompressedBytesPerReceive="1048576").
# Oracle: all 3000 decoded messages arrive and no guard message is logged, so the
# guards do not reject legitimate dense streams within the configured bounds.
. ${srcdir:=.}/diag.sh init
require_plugin imptcp
check_command_available python3

generate_conf
add_conf '
$MaxMessageSize 10k
global(processInternalMessages="on")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.maxExpansionRatio="1000"
	compression.maxDecompressedBytesPerReceive="1048576"
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

wait_file_lines "$RSYSLOG2_OUT_LOG" 3000
shutdown_when_empty
wait_shutdown

if [ "$(wc -l < "$RSYSLOG2_OUT_LOG")" -ne 3000 ]; then
	echo "FAIL: expected 3000 decompressed messages"
	error_exit 1
fi
if grep -q "decompressed bytes exceeded" "$RSYSLOG_OUT_LOG" 2>/dev/null; then
	echo "FAIL: guard unexpectedly triggered for a stream within limits"
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
