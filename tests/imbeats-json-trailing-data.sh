#!/bin/bash
# Regression coverage for strict JSON-frame consumption. A real imbeats
# listener receives frames whose declared payload is a valid object followed
# by either non-whitespace garbage or a second JSON value, plus non-object and
# malformed controls. A two-event window with a valid first event and malformed
# second event proves the entire batch is validated before any submission. The
# peer closing without an ACK is the rejection oracle; a timeout is a failure
# because it would leave the malformed session alive. A fresh, legitimate
# object must then be submitted and cumulatively ACKed, proving strict rejection
# did not damage normal Lumberjack v2 traffic.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%$!message%\n")

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

if ! $PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys

port = int(sys.argv[1])


def wire(payload, seq=1):
    return (
        b"2W" + struct.pack(">I", 1)
        + b"2J" + struct.pack(">I", seq)
        + struct.pack(">I", len(payload)) + payload
    )


def batch_wire(payloads):
    frames = [b"2W" + struct.pack(">I", len(payloads))]
    for seq, payload in enumerate(payloads, 1):
        frames.append(
            b"2J" + struct.pack(">I", seq)
            + struct.pack(">I", len(payload)) + payload
        )
    return b"".join(frames)


def require_wire_rejected(frame):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.settimeout(5)
        sock.sendall(frame)
        try:
            data = sock.recv(6)
        except (ConnectionResetError, BrokenPipeError):
            return
        except socket.timeout as exc:
            raise SystemExit("malformed JSON session was not closed") from exc
        if data:
            raise SystemExit(f"unexpected ACK for malformed JSON: {data.hex()}")


def require_rejected(payload):
    require_wire_rejected(wire(payload))


valid_prefix = b'{"message":"must-not-submit"}'
require_rejected(valid_prefix + b"garbage")
require_rejected(valid_prefix + b'{"message":"second-value"}')
require_rejected(b"[]")
require_rejected(b'"scalar"')
require_rejected(b'{"message":')
require_wire_rejected(batch_wire([b'{"message":"must-not-partially-submit"}', b'{"message":']))

payload = b" \t" + json.dumps({"message": "strict-json-control"}, separators=(",", ":")).encode() + b"\r\n"
with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.settimeout(5)
    sock.sendall(wire(payload))
    ack = sock.recv(6)
    if ack != b"2A" + struct.pack(">I", 1):
        raise SystemExit(f"unexpected control ACK: {ack.hex()}")
    print(ack.hex())
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

EXPECTED='strict-json-control'
cmp_exact

export EXPECTED='324100000001'
cmp_exact "$RSYSLOG_DYNNAME.imbeats.ack"

exit_test
