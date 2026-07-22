#!/bin/bash
# Verify the next window continues after a multi-event cumulative ACK instead
# of resetting sequence state.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string"
         string="%msg%|%$!metadata!imbeats!sequence%\n")

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

python3 - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys

port = int(sys.argv[1])


def frame(seq, message):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    return b"2J" + struct.pack(">I", seq) + struct.pack(">I", len(payload)) + payload


with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.settimeout(5)
    wire = b"2W" + struct.pack(">I", 2) + frame(1, "one") + frame(2, "two")
    sock.sendall(wire)
    print(sock.recv(6).hex())
    sock.sendall(b"2W" + struct.pack(">I", 1) + frame(3, "three"))
    print(sock.recv(6).hex())
PY

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"one"}|1
{"message":"two"}|2
{"message":"three"}|3'
cmp_exact

EXPECTED_ACK='324100000002
324100000003'
test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "$EXPECTED_ACK"

exit_test
