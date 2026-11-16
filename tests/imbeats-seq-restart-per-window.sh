#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that Lumberjack v2 sequence numbers are scoped to a window.
#
# Beats clients (go-lumber, used by Filebeat and Elastic Agent) number the
# events of every window 1..n and expect the ACK of that window to carry n.
# Earlier imbeats versions required numbering to continue across windows and
# reset the connection on the second window of a real Beats client.
#
# Stimulus: connection 1 sends three uncompressed windows (2, 3 and 1 events),
# each numbered from 1. Connection 2 sends two compressed windows (2 and 3
# events), each numbered from 1.
# Oracle: every window is ACKed with the sequence number of its own last event
# (the window's event count), all events are delivered exactly once and in
# order, and $!metadata!imbeats!sequence reports the wire sequence number.
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
import zlib

port = int(sys.argv[1])


def frame(seq, message):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    return b"2J" + struct.pack(">I", seq) + struct.pack(">I", len(payload)) + payload


def window(prefix, count):
    return b"".join(frame(i + 1, "%s-%d" % (prefix, i + 1)) for i in range(count))


def recv_ack(sock):
    data = b""
    while len(data) < 6:
        chunk = sock.recv(6 - len(data))
        if not chunk:
            break
        data += chunk
    return data.hex()


with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.settimeout(5)
    for prefix, count in (("a", 2), ("b", 3), ("c", 1)):
        sock.sendall(b"2W" + struct.pack(">I", count) + window(prefix, count))
        print(recv_ack(sock))

with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.settimeout(5)
    for prefix, count in (("z1", 2), ("z2", 3)):
        compressed = zlib.compress(window(prefix, count))
        sock.sendall(b"2W" + struct.pack(">I", count) + b"2C" + struct.pack(">I", len(compressed)) + compressed)
        print(recv_ack(sock))
PY

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"a-1"}|1
{"message":"a-2"}|2
{"message":"b-1"}|1
{"message":"b-2"}|2
{"message":"b-3"}|3
{"message":"c-1"}|1
{"message":"z1-1"}|1
{"message":"z1-2"}|2
{"message":"z2-1"}|1
{"message":"z2-2"}|2
{"message":"z2-3"}|3'
cmp_exact

EXPECTED_ACK='324100000002
324100000003
324100000001
324100000002
324100000003'
if [ "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" != "$EXPECTED_ACK" ]; then
	echo "FAIL: unexpected ACK sequence; expected:"
	echo "$EXPECTED_ACK"
	echo "got:"
	cat "$RSYSLOG_DYNNAME.imbeats.ack"
	error_exit 1
fi

exit_test
