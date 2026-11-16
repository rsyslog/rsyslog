#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: a client event whose top-level "metadata" member is not a
# JSON object (string, number, array, null) or whose metadata.imbeats member
# is not an object must be accepted like any other event. Module metadata is
# attached below $!metadata!imbeats after earlier events of the window were
# already submitted; a failure there used to close the session without ACK,
# so the sender retried forever and re-delivered the earlier events.
#
# Stimulus: ONE window containing normal events around five odd events.
# Oracle: (1) the window is ACKed with the last sequence number (seq 7),
# (2) the output file holds each of the 7 events exactly once, in order, and
# (3) $!metadata!imbeats!protocol is "lumberjack-v2" for every event, i.e. the
# module-owned metadata is present and was not taken from the client.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string"
         string="%msg%|%$!metadata!imbeats!protocol%|%$!metadata!imbeats!sequence%\n")

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

if ! python3 - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import socket
import struct
import sys

port = int(sys.argv[1])
payloads = [
    b'{"message":"first"}',
    b'{"message":"string","metadata":"x"}',
    b'{"message":"number","metadata":42}',
    b'{"message":"array","metadata":[1,2]}',
    b'{"message":"null","metadata":null}',
    b'{"message":"nested","metadata":{"imbeats":"x"}}',
    b'{"message":"last"}',
]
wire = b"2W" + struct.pack(">I", len(payloads))
for seq, payload in enumerate(payloads, 1):
    wire += b"2J" + struct.pack(">II", seq, len(payload)) + payload

with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
    sock.settimeout(10)
    sock.sendall(wire)
    got = b""
    while len(got) < 6:
        chunk = sock.recv(6 - len(got))
        if not chunk:
            break
        got += chunk
    sys.stdout.write(got.hex())
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"first"}|lumberjack-v2|1
{"message":"string","metadata":"x"}|lumberjack-v2|2
{"message":"number","metadata":42}|lumberjack-v2|3
{"message":"array","metadata":[1,2]}|lumberjack-v2|4
{"message":"null","metadata":null}|lumberjack-v2|5
{"message":"nested","metadata":{"imbeats":"x"}}|lumberjack-v2|6
{"message":"last"}|lumberjack-v2|7'
cmp_exact

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000007"

exit_test
