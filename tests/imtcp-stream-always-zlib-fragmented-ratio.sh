#!/bin/bash
# added 2026-05-22 by Codex, released under ASL 2.0
# Verifies that imtcp applies the expansion-ratio guard to the cumulative
# compressed stream, not each fragmented TCP receive. The oracle is that a
# valid stream sent one byte at a time is accepted with a low-but-legitimate
# ratio limit.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
check_command_available python3
export NUMMESSAGES=2000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zlib"
	compression.maxExpansionRatio="64")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

python3 - "$TCPFLOOD_PORT" <<'PY'
import socket
import sys
import time
import zlib

port = int(sys.argv[1])
payload = "".join(
    "<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:%08d AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n" % i
    for i in range(0, 2000)
).encode("ascii")
compressed = zlib.compress(payload, level=1)
sock = socket.create_connection(("127.0.0.1", port))
sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
for octet in compressed:
    sock.sendall(bytes((octet,)))
    time.sleep(0.0005)
sock.close()
PY

shutdown_when_empty
wait_shutdown
seq_check
exit_test
