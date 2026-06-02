#!/bin/bash
. ${srcdir:=.}/diag.sh init
require_plugin imbeats

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
      maxSessions="8"
      workerThreads="2"
      starvationProtection.maxReads="1")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys

port = int(sys.argv[1])
sockets = []

try:
    for _ in range(5):
        sock = socket.create_connection(("127.0.0.1", port), timeout=5)
        sock.settimeout(5)
        sockets.append(sock)

    payload = json.dumps({"message": "active-after-idle"}, separators=(",", ":")).encode()
    wire = (
        b"2W" + struct.pack(">I", 1)
        + b"2J" + struct.pack(">I", 1)
        + struct.pack(">I", len(payload)) + payload
    )
    active = sockets[-1]
    active.sendall(wire)
    ack = active.recv(6)
    print(ack.hex())
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

EXPECTED='{"message":"active-after-idle"}'
cmp_exact

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000001" || error_exit 1

exit_test
