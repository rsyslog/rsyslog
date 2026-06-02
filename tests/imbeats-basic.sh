#!/bin/bash
. ${srcdir:=.}/diag.sh init
require_plugin imbeats

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string"
         string="%msg%|%$!message%|%$!host!name%|%$!metadata!imbeats!protocol%|%$!metadata!imbeats!sequence%\n")

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
payload = json.dumps({"message": "hello", "host": {"name": "web01"}}, separators=(",", ":")).encode()
batch = b"2W" + struct.pack(">I", 1)
event = b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(payload)) + payload

sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(batch + event)
ack = sock.recv(6)
sys.stdout.write(ack.hex())
sock.close()
PY

shutdown_when_empty
wait_shutdown

EXPECTED='{"message":"hello","host":{"name":"web01"}}|hello|web01|lumberjack-v2|1'
cmp_exact

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000001"

exit_test
