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
      ruleset="main")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

rm -f "$RSYSLOG_DYNNAME.imbeats.ready" "$RSYSLOG_DYNNAME.imbeats.client-result"

${PYTHON:-python3} - "$RS_PORT" \
	"$RSYSLOG_DYNNAME.imbeats.ready" \
	"$RSYSLOG_DYNNAME.imbeats.client-result" <<'PY' &
import os
import socket
import struct
import sys

port = int(sys.argv[1])
ready_file = sys.argv[2]
result_file = sys.argv[3]
sockets = []

try:
    for idx in range(3):
        sock = socket.create_connection(("127.0.0.1", port), timeout=5)
        sock.settimeout(10)
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

for i in {1..50}; do
	if [ -f "$RSYSLOG_DYNNAME.imbeats.ready" ]; then
		break
	fi
	$TESTTOOL_DIR/msleep 100
done

if [ ! -f "$RSYSLOG_DYNNAME.imbeats.ready" ]; then
	kill "$client_pid" 2>/dev/null || true
	error_exit 1
fi

shutdown_when_empty
wait_shutdown "" 10
wait "$client_pid" || error_exit 1

test "$(cat "$RSYSLOG_DYNNAME.imbeats.client-result")" = "closed" || error_exit 1

exit_test
