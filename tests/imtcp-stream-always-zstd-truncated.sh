#!/bin/bash
# added 2026-05-17 by Codex, released under ASL 2.0
# Sends a truncated zstd stream that contains one syslog message; imtcp must
# preserve that message and log the truncated stream.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime
check_command_available python3
check_command_available zstd
wait_truncated_stream_result() {
	content_check --check-only "imtcp: detected truncated compressed stream" "$RSYSLOG_OUT_LOG" || return 1
	[ -f "$RSYSLOG2_OUT_LOG" ] || return 1
	[ "$(wc -l < "$RSYSLOG2_OUT_LOG")" -eq 1 ]
}
export QUEUE_EMPTY_CHECK_FUNC=wait_truncated_stream_result

generate_conf
add_conf '
$MaxMessageSize 10k
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zstd")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
	stop
}
action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

printf '<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\n' > "$RSYSLOG_DYNNAME.rawmsg"
zstd -q -c "$RSYSLOG_DYNNAME.rawmsg" > "$RSYSLOG_DYNNAME.zst"

python3 - "$TCPFLOOD_PORT" "$RSYSLOG_DYNNAME.zst" <<'PY'
import socket
import sys

port = int(sys.argv[1])
path = sys.argv[2]
with open(path, "rb") as stream:
    payload = stream.read()
sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(payload[:-1])
sock.close()
PY

shutdown_when_empty
wait_shutdown

content_check "imtcp: detected truncated compressed stream"
if [ ! -f "$RSYSLOG2_OUT_LOG" ] || [ "$(cat "$RSYSLOG2_OUT_LOG")" != "1" ]; then
	echo "FAIL: truncated compressed stream did not preserve already decompressed message"
	test -f "$RSYSLOG2_OUT_LOG" && cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
