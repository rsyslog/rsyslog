#!/bin/bash
# Regression test for imbeats timeout ownership while a complete batch is
# submitted. A direct ruleset deliberately spends longer than idleTimeout on
# each event, and starvation protection yields between the two submissions.
# The cumulative ACK and both output lines prove that neither the timeout
# scanner nor worker re-entry expired the session during local submission.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")
module(load="../plugins/omtesting/.libs/omtesting")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="main" queue.type="Direct") {
  :omtesting:sleep 1 100000
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main"
      idleTimeout="1"
      workerThreads="1"
      starvationProtection.maxReads="1")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys

port = int(sys.argv[1])
events = []


def recv_exactly(sock, length):
    data = b""
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise ConnectionError("connection closed before cumulative ACK")
        data += chunk
    return data


for sequence in (1, 2):
    payload = json.dumps({"message": "submit-timeout-%d" % sequence}, separators=(",", ":")).encode()
    events.append(b"2J" + struct.pack(">I", sequence) + struct.pack(">I", len(payload)) + payload)

with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.settimeout(10)
    sock.sendall(b"2W" + struct.pack(">I", len(events)) + b"".join(events))
    print(recv_exactly(sock, 6).hex())
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

# diag.sh's cmp_exact consumes this conventionally named variable.
# shellcheck disable=SC2034
EXPECTED='{"message":"submit-timeout-1"}
{"message":"submit-timeout-2"}'
cmp_exact

# shellcheck disable=SC2034
EXPECTED='324100000002'
cmp_exact "$RSYSLOG_DYNNAME.imbeats.ack"

exit_test
