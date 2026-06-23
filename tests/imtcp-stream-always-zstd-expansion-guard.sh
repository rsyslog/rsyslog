#!/bin/bash
# added 2026-05-19 by Codex, released under ASL 2.0
# Sends a valid but highly expanding zstd stream; the oracle is that imtcp logs
# an invalid compressed stream before submitting decompressed messages.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime
check_command_available python3
check_command_available zstd
wait_invalid_stream_log() {
	content_check --check-only "decompressed bytes exceeded configured expansion ratio" "$RSYSLOG_OUT_LOG"
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
	compression.maxExpansionRatio="2")

if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
	stop
}
action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

python3 - "$RSYSLOG_DYNNAME.rawmsg" <<'PY'
import sys

path = sys.argv[1]
line = b"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
with open(path, "wb") as stream:
    stream.write(line * 3000)
PY
zstd -q -19 -c "$RSYSLOG_DYNNAME.rawmsg" > "$RSYSLOG_DYNNAME.zst"

python3 - "$TCPFLOOD_PORT" "$RSYSLOG_DYNNAME.zst" <<'PY'
import socket
import sys

port = int(sys.argv[1])
path = sys.argv[2]
with open(path, "rb") as stream:
    payload = stream.read()
sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(payload)
sock.close()
PY

shutdown_when_empty
wait_shutdown

content_check "decompressed bytes exceeded configured expansion ratio"
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: expansion-guarded zstd stream unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
