#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that a window whose sequence numbers are not consecutive is rejected
# as a whole: no event of that window is submitted and no ACK is sent.
#
# Sequence numbers of a window must be 1..n (Beats clients) or continue from
# the last ACKed sequence number (cumulative senders), and must increase by
# exactly one per event. Three connections each first send a valid window
# (seq 1..2, ACKed with 2) and then an invalid 3-event window:
#   gap:     1, 3, 4
#   order:   1, 3, 2
#   start:   4, 5, 6  (neither 1 nor last ACK + 1)
# Oracle: each connection receives exactly the ACK for its valid window; the
# invalid window gets no ACK (the server closes the connection, seen by the
# client as EOF, reset or a 5 s receive timeout), and the output contains only
# the events of the valid windows, so nothing of an invalid window was
# partially accepted.
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


def recv_ack(sock):
    data = b""
    try:
        while len(data) < 6:
            chunk = sock.recv(6 - len(data))
            if not chunk:
                break
            data += chunk
    except (ConnectionResetError, TimeoutError, socket.timeout):
        pass
    return data.hex() if data else "none"


for name, seqs in (("gap", (1, 3, 4)), ("order", (1, 3, 2)), ("start", (4, 5, 6))):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.settimeout(5)
        sock.sendall(b"2W" + struct.pack(">I", 2) + frame(1, name + "-ok-1") + frame(2, name + "-ok-2"))
        print(name, recv_ack(sock))
        bad = b"".join(frame(seq, "%s-bad-%d" % (name, seq)) for seq in seqs)
        try:
            sock.sendall(b"2W" + struct.pack(">I", len(seqs)) + bad)
        except (BrokenPipeError, ConnectionResetError):
            pass
        print(name, recv_ack(sock))
PY

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"gap-ok-1"}|1
{"message":"gap-ok-2"}|2
{"message":"order-ok-1"}|1
{"message":"order-ok-2"}|2
{"message":"start-ok-1"}|1
{"message":"start-ok-2"}|2'
cmp_exact

EXPECTED_ACK='gap 324100000002
gap none
order 324100000002
order none
start 324100000002
start none'
if [ "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" != "$EXPECTED_ACK" ]; then
	echo "FAIL: unexpected ACK sequence; expected:"
	echo "$EXPECTED_ACK"
	echo "got:"
	cat "$RSYSLOG_DYNNAME.imbeats.ack"
	error_exit 1
fi

exit_test
