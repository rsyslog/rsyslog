#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify the default maxWindowSize of imbeats: a window of 2048 events (the
# default bulk_max_size of the Beats Logstash output) is accepted when no
# maxWindowSize is configured, and a window announcing 2049 events is
# rejected.
#
# Oracle: (1) the 2048-event window is ACKed with sequence number 2048 and the
# output file holds exactly 2048 events; (2) a connection announcing a window
# of 2049 events is closed by the server without any ACK (peer sees EOF or a
# reset, never data) and delivers no event. The server-side rejection is
# immediate, so the 15 s read timeout only bounds a hang.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

# no maxWindowSize: the compiled-in default is under test
input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import socket
import struct
import sys

port = int(sys.argv[1])
payload = b'{"message":"x"}'

# 2048 events in one window: must be ACKed
wire = b"2W" + struct.pack(">I", 2048)
for seq in range(1, 2049):
    wire += b"2J" + struct.pack(">II", seq, len(payload)) + payload
with socket.create_connection(("127.0.0.1", port), timeout=15) as sock:
    sock.settimeout(15)
    sock.sendall(wire)
    got = b""
    while len(got) < 6:
        chunk = sock.recv(6 - len(got))
        if not chunk:
            break
        got += chunk
print(got.hex())

# a window announcing 2049 events: must be closed without ACK
with socket.create_connection(("127.0.0.1", port), timeout=15) as sock:
    sock.settimeout(15)
    try:
        sock.sendall(b"2W" + struct.pack(">I", 2049))
        data = sock.recv(6)
    except (ConnectionResetError, BrokenPipeError):
        data = b""
    except (TimeoutError, socket.timeout):
        raise SystemExit("server did not close the connection for a 2049-event window")
    if data:
        raise SystemExit(f"unexpected data for rejected window: {data.hex()}")
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000800" || { echo "FAIL: unexpected ACK: $(cat "$RSYSLOG_DYNNAME.imbeats.ack")"; error_exit 1; }
n=$(wc -l < "$RSYSLOG_OUT_LOG")
test "$n" -eq 2048 || { echo "FAIL: expected 2048 events, got $n"; error_exit 1; }

exit_test
