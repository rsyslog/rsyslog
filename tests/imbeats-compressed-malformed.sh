#!/bin/bash
. ${srcdir:=.}/diag.sh init
require_plugin imbeats
require_plugin impstats

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
      ruleset="main")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

$PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys
import zlib

port = int(sys.argv[1])


def frame(seq, message):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    return b"2J" + struct.pack(">I", seq) + struct.pack(">I", len(payload)) + payload


def send_bad(compressed):
    wire = b"2W" + struct.pack(">I", 1) + b"2C" + struct.pack(">I", len(compressed)) + compressed
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.settimeout(5)
        sock.sendall(wire)
        try:
            data = sock.recv(6)
        except (ConnectionResetError, TimeoutError, socket.timeout):
            data = b""
        if data:
            raise SystemExit(f"unexpected ack for malformed compressed frame: {data.hex()}")


send_bad(b"not-a-zlib-stream")
send_bad(zlib.compress(frame(1, "ok")) + b"trailing-garbage")

nested = b"2C" + struct.pack(">I", len(zlib.compress(frame(1, "nested")))) + zlib.compress(frame(1, "nested"))
send_bad(zlib.compress(nested))

zero = b"2J" + struct.pack(">I", 1) + struct.pack(">I", 0)
send_bad(zlib.compress(zero))
PY

rst_msleep 1500

shutdown_when_empty
wait_shutdown

test ! -s "$RSYSLOG_OUT_LOG"
test ! -s "$RSYSLOG_DYNNAME.imbeats.ack"

if ! $PYTHON - "$RSYSLOG_DYNNAME.spool/imbeats-stats.log" <<'PY'
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
    "compressed_frames": 4,
    "compressed.rejected": 4,
    "protocol_errors": 4,
    "events.submitted": 0,
}
missing = {key: (stats.get(key), value) for key, value in expected.items() if stats.get(key) != value}
if missing:
    raise SystemExit(f"unexpected imbeats stats: {missing}; last stats={stats}")
PY
then
	error_exit 1
fi

exit_test
