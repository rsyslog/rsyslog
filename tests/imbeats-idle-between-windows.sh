#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression: a peer that is quiet between two Lumberjack windows must not be
# disconnected by frameTimeout. frameTimeout bounds a partially received frame
# only; an idle connection is governed by idleTimeout. Here frameTimeout=1s and
# idleTimeout=30s; the client waits 3s between windows and must still get the
# second ACK on the same connection (oracle: both ACKs received, no EOF).
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main"
      frameTimeout="1"
      idleTimeout="30")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

$PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY' || error_exit 1
import json
import socket
import struct
import sys
import time

port = int(sys.argv[1])


def frame(seq, message):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    return b"2J" + struct.pack(">I", seq) + struct.pack(">I", len(payload)) + payload


def recv_ack(sock):
    data = b""
    while len(data) < 6:
        chunk = sock.recv(6 - len(data))
        if not chunk:
            raise EOFError("connection closed before full ACK")
        data += chunk
    return data


with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
    sock.settimeout(10)
    sock.sendall(b"2W" + struct.pack(">I", 1) + frame(1, "first"))
    print(recv_ack(sock).hex())
    time.sleep(3)
    sock.sendall(b"2W" + struct.pack(">I", 1) + frame(2, "second"))
    print(recv_ack(sock).hex())
PY

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"first"}
{"message":"second"}'
cmp_exact

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000001
324100000002" || error_exit 1

exit_test
