#!/bin/bash
# Verify client JSON cannot overwrite the reserved imbeats protocol metadata
# attached during message submission.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string"
         string="%msg%|%$!metadata!imbeats!protocol%|%$!metadata!imbeats!sequence%|%$!metadata!imbeats!tls_enabled%\n")

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
payload = json.dumps(
    {
        "message": "metadata collision",
        "metadata": {
            "imbeats": {
                "protocol": "attacker",
                "sequence": 999,
                "tls_enabled": True,
            }
        },
    },
    separators=(",", ":"),
).encode()
wire = b"2W" + struct.pack(">I", 1) + b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(payload)) + payload

sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(wire)
ack = sock.recv(6)
sys.stdout.write(ack.hex())
sock.close()
PY

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"metadata collision","metadata":{"imbeats":{"protocol":"attacker","sequence":999,"tls_enabled":true}}}|lumberjack-v2|1|false'
cmp_exact

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000001"

exit_test
