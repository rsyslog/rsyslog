#!/bin/bash
# Exercise imbeats' listener-wide in-flight budget and compression-ratio cap.
# One connection declares an incomplete 6144-byte JSON body under an 8192-byte
# aggregate budget. Bounded retries wait until a second 4096-byte reservation is
# rejected by peer close (never ACK); this avoids assuming when the worker has
# consumed the first header. Closing the holder must release its reservation,
# proven by a legitimate direct event receiving an ACK within the same bounded
# retry window. A high-ratio compressed frame must also close without an ACK,
# while a low-ratio compressed control remains accepted. Five seconds permits
# loaded CI scheduling without turning silence/timeouts into a passing oracle.
. ${srcdir:=.}/diag.sh init

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
      workerThreads="1"
      maxInFlightBytes="8192"
      maxCompressionRatio="4")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import select
import socket
import struct
import sys
import time
import zlib

port = int(sys.argv[1])
ack1 = b"2A" + struct.pack(">I", 1)


def window(frame):
    return b"2W" + struct.pack(">I", 1) + frame


def json_frame(payload, seq=1):
    return b"2J" + struct.pack(">I", seq) + struct.pack(">I", len(payload)) + payload


def declared_json(length, partial=b""):
    return b"2J" + struct.pack(">I", 1) + struct.pack(">I", length) + partial


def peer_closed(sock, timeout):
    readable, _, _ = select.select([sock], [], [], timeout)
    if not readable:
        return False
    try:
        return sock.recv(6) == b""
    except (ConnectionResetError, BrokenPipeError):
        return True


def retry_until_ack(frame, deadline):
    last = b""
    while time.monotonic() < deadline:
        with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
            sock.settimeout(1)
            sock.sendall(window(frame))
            try:
                last = sock.recv(6)
            except (ConnectionResetError, BrokenPipeError, socket.timeout):
                last = b""
            if last == ack1:
                return last
        select.select([], [], [], 0.05)
    raise SystemExit(f"legitimate frame was not ACKed after budget release; last={last.hex()}")


holder = socket.create_connection(("127.0.0.1", port), timeout=5)
holder.settimeout(2)
try:
    holder.sendall(window(declared_json(6144, b"{")))

    # Admission and parsing are asynchronous. Retry the competing reservation
    # until the server's close proves the holder has been charged.
    deadline = time.monotonic() + 5
    rejected = False
    while time.monotonic() < deadline and not rejected:
        with socket.create_connection(("127.0.0.1", port), timeout=2) as contender:
            contender.sendall(window(declared_json(4096, b"{")))
            rejected = peer_closed(contender, 0.25)
        if not rejected:
            select.select([], [], [], 0.05)
    if not rejected:
        raise SystemExit("aggregate in-flight budget did not reject a competing reservation")
finally:
    holder.close()

direct = json.dumps({"message": "budget-released"}, separators=(",", ":")).encode()
direct_ack = retry_until_ack(json_frame(direct), time.monotonic() + 5)

high = json_frame(json.dumps({"message": "x" * 400}, separators=(",", ":")).encode())
high_compressed = zlib.compress(high)
if len(high) <= 4 * len(high_compressed):
    raise SystemExit("test bug: high-ratio fixture does not exceed configured ratio")
with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.sendall(window(b"2C" + struct.pack(">I", len(high_compressed)) + high_compressed))
    if not peer_closed(sock, 5):
        raise SystemExit("high-ratio compressed frame was not rejected by peer close")

normal = json_frame(
    json.dumps({"message": "compressed-control-0123456789abcdef"}, separators=(",", ":")).encode()
)
normal_compressed = zlib.compress(normal)
if len(normal) > 4 * len(normal_compressed):
    raise SystemExit("test bug: normal compressed fixture exceeds configured ratio")
compressed_ack = retry_until_ack(
    b"2C" + struct.pack(">I", len(normal_compressed)) + normal_compressed,
    time.monotonic() + 5,
)

print(direct_ack.hex())
print(compressed_ack.hex())
PY
then
	error_exit 1
fi

rst_msleep 1500

shutdown_when_empty
wait_shutdown

EXPECTED='{"message":"budget-released"}
{"message":"compressed-control-0123456789abcdef"}'
cmp_exact

EXPECTED_ACK='324100000001
324100000001'
export EXPECTED="$EXPECTED_ACK"
cmp_exact "$RSYSLOG_DYNNAME.imbeats.ack"

if ! $PYTHON - "$RSYSLOG_DYNNAME.spool/imbeats-stats.log" <<'PY'
import json
import sys

stats = {}
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        if "@cee: " in line:
            data = json.loads(line.split("@cee: ", 1)[1])
            if data.get("name") == "imbeats":
                stats = data

if stats.get("resource_bytes") != 0:
    raise SystemExit(f"resource accounting did not return to zero: {stats}")
if stats.get("resource_rejected", 0) < 1:
    raise SystemExit(f"resource rejection counter did not advance: {stats}")
if stats.get("protocol_errors") != 1:
    raise SystemExit(f"resource rejection was misclassified as a protocol error: {stats}")
PY
then
	error_exit 1
fi

exit_test
