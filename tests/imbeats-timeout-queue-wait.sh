#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: imbeats deadlines must not count the time a session waits
# in the work queue for a free worker after the peer has already sent data.
#
# Setup: one worker thread, idleTimeout=2s, and a Direct ruleset that writes
# each event and then sleeps 1.1s (omtesting), so the worker itself spends
# about 1.1s per event in submission.
# Stimulus: connection A sends a 4-event window. As soon as A's first event
# is in the output file, the single worker is known to be busy with A for
# roughly 3 * 1.1s + the rest of the first sleep, i.e. about 4s, which is
# about 2s more than idleTimeout. Connection B then connects and immediately
# sends a complete 1-event window; it waits in the work queue for that time.
# Oracle: A is ACKed with 4 and B is ACKed with 1, and all five events are
# written. Before the fix the worker evaluated B's idle deadline from the
# accept time when it finally picked B up, closed B as idle and never ACKed
# it (the client sees EOF or reset). The 2s margin over idleTimeout and the
# 20s client receive timeout keep the test stable on slow hosts; the sleeps
# are fixed, so a slower host only lengthens B's queue wait.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")
module(load="../plugins/omtesting/.libs/omtesting")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="main" queue.type="Direct") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
  :omtesting:sleep 1 100000
}

input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main"
      idleTimeout="2"
      workerThreads="1")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import os
import socket
import struct
import sys
import time

port = int(sys.argv[1])
out_log = sys.argv[2]


def window(prefix, count):
    frames = []
    for seq in range(1, count + 1):
        payload = json.dumps({"message": "%s-%d" % (prefix, seq)}, separators=(",", ":")).encode()
        frames.append(b"2J" + struct.pack(">I", seq) + struct.pack(">I", len(payload)) + payload)
    return b"2W" + struct.pack(">I", count) + b"".join(frames)


def recv_ack(sock, name):
    data = b""
    try:
        while len(data) < 6:
            chunk = sock.recv(6 - len(data))
            if not chunk:
                break
            data += chunk
    except (ConnectionResetError, TimeoutError, socket.timeout) as exc:
        print("%s error %s" % (name, type(exc).__name__))
        return
    print("%s %s" % (name, data.hex() if data else "eof"))


sock_a = socket.create_connection(("127.0.0.1", port), timeout=20)
sock_a.settimeout(20)
sock_a.sendall(window("busy", 4))

# Wait until the worker is inside A's submission (first event written).
deadline = time.monotonic() + 20
while time.monotonic() < deadline:
    if os.path.exists(out_log) and os.path.getsize(out_log) > 0:
        break
    time.sleep(0.05)
else:
    sys.exit("worker never started submitting connection A")

sock_b = socket.create_connection(("127.0.0.1", port), timeout=20)
sock_b.settimeout(20)
sock_b.sendall(window("queued", 1))

recv_ack(sock_a, "A")
recv_ack(sock_b, "B")
sock_a.close()
sock_b.close()
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

# diag.sh's cmp_exact consumes this conventionally named variable.
# shellcheck disable=SC2034
EXPECTED='{"message":"busy-1"}
{"message":"busy-2"}
{"message":"busy-3"}
{"message":"busy-4"}
{"message":"queued-1"}'
cmp_exact

# shellcheck disable=SC2034
EXPECTED='A 324100000004
B 324100000001'
cmp_exact "$RSYSLOG_DYNNAME.imbeats.ack"

exit_test
