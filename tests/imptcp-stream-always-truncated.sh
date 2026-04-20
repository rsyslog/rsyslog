#!/bin/bash
# added 2026-04-20 by Codex, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
$MaxMessageSize 10k
global(processInternalMessages="on")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="outfmt")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

python3 - "$TCPFLOOD_PORT" <<'PY'
import socket
import sys
import zlib

port = int(sys.argv[1])
msg = b"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\n"
comp = zlib.compressobj(level=6, method=zlib.DEFLATED, wbits=zlib.MAX_WBITS)
payload = comp.compress(msg) + comp.flush(zlib.Z_SYNC_FLUSH)
sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(payload)
sock.close()
PY

sleep 1
shutdown_when_empty
wait_shutdown

content_check "imptcp: detected truncated compressed stream"
if [ ! -f "$RSYSLOG2_OUT_LOG" ] || [ "$(cat "$RSYSLOG2_OUT_LOG")" != "1" ]; then
	echo "FAIL: truncated compressed stream did not preserve already decompressed message"
	test -f "$RSYSLOG2_OUT_LOG" && cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
