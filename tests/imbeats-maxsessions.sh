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
      maxSessions="2")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys

port = int(sys.argv[1])


def event_wire(seq, message):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    return (
        b"2W" + struct.pack(">I", 1)
        + b"2J" + struct.pack(">I", seq)
        + struct.pack(">I", len(payload)) + payload
    )


sockets = []
try:
    for _ in range(2):
        sock = socket.create_connection(("127.0.0.1", port), timeout=5)
        sock.settimeout(5)
        sockets.append(sock)

    try:
        extra = socket.create_connection(("127.0.0.1", port), timeout=5)
    except OSError:
        extra = None
    if extra is not None:
        with extra:
            extra.settimeout(2)
            extra.sendall(event_wire(99, "rejected"))
            try:
                data = extra.recv(6)
            except (ConnectionResetError, TimeoutError, socket.timeout):
                data = b""
            if data:
                raise SystemExit(f"unexpected ack from over-limit session: {data.hex()}")

    acks = []
    for idx, sock in enumerate(sockets, start=1):
        sock.sendall(event_wire(1, f"accepted-{idx}"))
        acks.append(sock.recv(6).hex())
    print("\n".join(acks))
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

EXPECTED='{"message":"accepted-1"}
{"message":"accepted-2"}'
cmp_exact

grep -Fx "324100000001" "$RSYSLOG_DYNNAME.imbeats.ack" >/dev/null || error_exit 1
test "$(grep -Fx "324100000001" "$RSYSLOG_DYNNAME.imbeats.ack" | wc -l)" -eq 2 || error_exit 1

exit_test
