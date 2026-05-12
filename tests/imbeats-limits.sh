#!/bin/bash
. ${srcdir:=.}/diag.sh init
require_plugin imbeats

generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats" interval="1"
       log.file="'$RSYSLOG_DYNNAME'.spool/imbeats-stats.log"
       log.syslog="off" format="cee")
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main"
      maxWindowSize="2"
      maxFrameSize="64"
      maxDecompressedSize="128"
      maxBatchBytes="35")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! python3 - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys
import zlib

port = int(sys.argv[1])


def send_bad(wire):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.settimeout(5)
        sock.sendall(wire)
        try:
            data = sock.recv(6)
        except (ConnectionResetError, TimeoutError, socket.timeout):
            data = b""
        if data:
            raise SystemExit(f"unexpected ack for rejected frame: {data.hex()}")


def send_good(wire):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.settimeout(5)
        sock.sendall(wire)
        return sock.recv(6).hex()


send_bad(b"2W" + struct.pack(">I", 3))
send_bad(b"2W" + struct.pack(">I", 1) + b"2J" + struct.pack(">I", 1) + struct.pack(">I", 65))
send_bad(b"2W" + struct.pack(">I", 1) + b"2C" + struct.pack(">I", 65))

payload1 = json.dumps({"message": "first"}, separators=(",", ":")).encode()
payload2 = json.dumps({"message": "second"}, separators=(",", ":")).encode()
send_bad(
    b"2W" + struct.pack(">I", 2)
    + b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(payload1)) + payload1
    + b"2J" + struct.pack(">I", 2) + struct.pack(">I", len(payload2)) + payload2
)

large_payload = json.dumps({"message": "x" * 140}, separators=(",", ":")).encode()
compressed = zlib.compress(b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(large_payload)) + large_payload)
if len(compressed) > 64:
    raise SystemExit(f"test bug: compressed payload is {len(compressed)} bytes")
send_bad(b"2W" + struct.pack(">I", 1) + b"2C" + struct.pack(">I", len(compressed)) + compressed)

payload = json.dumps({"message": "ok"}, separators=(",", ":")).encode()
ack = send_good(b"2W" + struct.pack(">I", 1) + b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(payload)) + payload)
print(ack)
PY
then
	error_exit 1
fi

rst_msleep 1500

shutdown_when_empty
wait_shutdown

EXPECTED='{"message":"ok"}'
cmp_exact

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000001"

if ! $PYTHON - "$srcdir/$RSYSLOG_DYNNAME.spool/imbeats-stats.log" <<'PY'
import json
import sys

stats = {}
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        if "@cee: " not in line:
            continue
        line = line.split("@cee: ", 1)[1]
        data = json.loads(line)
        if data.get("name") == "imbeats":
            stats = data

expected = {
    "windows.rejected": 1,
    "frames.rejected": 3,
    "compressed.rejected": 2,
    "protocol_errors": 5,
    "events.submitted": 1,
}
missing = {key: (stats.get(key), value) for key, value in expected.items() if stats.get(key) != value}
if missing:
    raise SystemExit(f"unexpected imbeats stats: {missing}; last stats={stats}")
PY
then
	error_exit 1
fi

exit_test
