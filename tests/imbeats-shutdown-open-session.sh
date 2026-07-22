#!/bin/bash
. ${srcdir:=.}/diag.sh init

# This checks that imbeats shutdown closes client sessions that are already
# accepted and left open in idle or partial-frame protocol states. The client
# first completes one valid Lumberjack batch on each socket and waits for the
# expected ack; that ack is the oracle that the session is owned by imbeats
# before the test starts rsyslog shutdown.

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
      ruleset="main")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

rm -f "$RSYSLOG_DYNNAME.imbeats.ready" "$RSYSLOG_DYNNAME.imbeats.client-result"

${PYTHON:-python3} - "$RS_PORT" \
	"$RSYSLOG_DYNNAME.imbeats.ready" \
	"$RSYSLOG_DYNNAME.imbeats.client-result" <<'PY' &
import json
import os
import socket
import struct
import sys

port = int(sys.argv[1])
ready_file = sys.argv[2]
result_file = sys.argv[3]
sockets = []

def event_wire(seq, message):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    return (
        b"2W" + struct.pack(">I", 1)
        + b"2J" + struct.pack(">I", seq)
        + struct.pack(">I", len(payload)) + payload
    )

def recv_exact(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise SystemExit("server closed session before ack")
        data += chunk
    return data

try:
    for idx in range(3):
        sock = socket.create_connection(("127.0.0.1", port), timeout=5)
        sock.settimeout(10)
        sock.sendall(event_wire(1, f"accepted-{idx}"))
        ack = recv_exact(sock, 6)
        if ack != b"2A" + struct.pack(">I", 1):
            raise SystemExit(f"unexpected ack before shutdown: {ack.hex()}")
        sockets.append(sock)
        if idx == 1:
            sock.sendall(b"2W" + struct.pack(">I", 1))
        elif idx == 2:
            sock.sendall(b"2W" + struct.pack(">I", 1) + b"2J" + struct.pack(">I", 1))

    with open(ready_file, "w", encoding="ascii") as fh:
        fh.write("ready\n")

    for sock in sockets:
        try:
            data = sock.recv(1)
        except socket.timeout:
            raise SystemExit("server did not close open session during shutdown")
        except (ConnectionResetError, OSError):
            data = b""
        if data:
            raise SystemExit(f"unexpected data from server during shutdown: {data!r}")

    with open(result_file, "w", encoding="ascii") as fh:
        fh.write("closed\n")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    if not os.path.exists(result_file):
        with open(result_file, "w", encoding="ascii") as fh:
            fh.write("failed\n")
PY
client_pid=$!

i=0
ready_timeout="${TB_TIMEOUT_STARTSTOP:-600}"
while [ "$i" -lt "$ready_timeout" ]; do
	if [ -f "$RSYSLOG_DYNNAME.imbeats.ready" ]; then
		break
	fi
	if ! kill -0 "$client_pid" 2>/dev/null; then
		wait "$client_pid" || true
		cat "$RSYSLOG_DYNNAME.imbeats.client-result" 2>/dev/null || true
		error_exit 1
	fi
	$TESTTOOL_DIR/msleep 100
	i=$((i + 1))
done

if [ ! -f "$RSYSLOG_DYNNAME.imbeats.ready" ]; then
	kill "$client_pid" 2>/dev/null || true
	cat "$RSYSLOG_DYNNAME.imbeats.client-result" 2>/dev/null || true
	error_exit 1
fi

shutdown_when_empty
wait_shutdown "" 10
wait "$client_pid" || error_exit 1

test "$(cat "$RSYSLOG_DYNNAME.imbeats.client-result")" = "closed" || error_exit 1

exit_test
