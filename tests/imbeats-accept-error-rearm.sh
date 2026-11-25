#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Verify that an imbeats listener recovers after a transient accept(2) error.
# The listener keeps itself disarmed after an accept error (to avoid an epoll
# busy loop on the still-pending connection); it used to stay disarmed
# forever, so a short file-descriptor shortage stopped all new connections
# until restart.
#
# Stimulus: after startup, the rsyslogd open-files limit is lowered with
# prlimit to just above its current descriptor count, and the test opens more
# idle connections than fit. accept(2) then fails with EMFILE.
# Oracle: (1) the nsd_ptcp "error accepting connection" warning reaches the
# default-ruleset log, proving the error path was exercised; (2) after the
# limit is restored and the idle connections are closed, a new Lumberjack
# event must receive its ACK and reach the output file. The ACK wait timeout
# only bounds a listener that never rearms; the fixed listener rearms about
# one second after the error.
# Both output files are opened before the limit is lowered, so the error
# message and the final event can be written while descriptors are scarce.
. ${srcdir:=.}/diag.sh init
if ! command -v prlimit > /dev/null 2>&1 || [ ! -d /proc/self/fd ]; then
	echo "SKIP: prlimit or /proc/self/fd not available"
	exit 77
fi

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

if $msg contains "start" or $msg contains "error accepting connection" then
  action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")

input(type="imbeats"
      address="127.0.0.1"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"
RS_PID=$(getpid)
# wait for the internal startup message so the second output file is open
wait_file_lines "$RSYSLOG2_OUT_LOG" 1 50

if ! $PYTHON - "$RS_PORT" "$RS_PID" "$RSYSLOG2_OUT_LOG" <<'PY'
import json
import os
import socket
import struct
import subprocess
import sys
import time

port = int(sys.argv[1])
pid = sys.argv[2]
errlog = sys.argv[3]


def send_event(message, timeout):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    wire = b"2W" + struct.pack(">I", 1)
    wire += b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(payload)) + payload
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(wire)
        ack = b""
        while len(ack) < 6:
            chunk = sock.recv(6 - len(ack))
            if not chunk:
                raise SystemExit("listener closed before its Lumberjack ACK")
            ack += chunk
        if ack != b"2A" + struct.pack(">I", 1):
            raise SystemExit(f"unexpected ack: {ack.hex()}")


# Open the main output file before descriptors become scarce.
send_event("before-emfile", 10)

orig = subprocess.run(["prlimit", "--pid", pid, "--nofile", "--raw", "--noheadings", "--output", "SOFT,HARD"],
                      check=True, capture_output=True, text=True).stdout.split()
inuse = len(os.listdir(f"/proc/{pid}/fd"))
subprocess.run(["prlimit", "--pid", pid, f"--nofile={inuse + 2}:{orig[1]}"], check=True)

idle = []
try:
    for _ in range(8):
        idle.append(socket.create_connection(("127.0.0.1", port), timeout=5))
    deadline = time.time() + 15
    while time.time() < deadline:
        with open(errlog, encoding="utf-8", errors="replace") as f:
            if "error accepting connection" in f.read():
                break
        time.sleep(0.1)
    else:
        raise SystemExit("accept error was not triggered")
finally:
    subprocess.run(["prlimit", "--pid", pid, f"--nofile={orig[0]}:{orig[1]}"], check=True)
    for s in idle:
        s.close()

try:
    send_event("after-emfile", 15)
except socket.timeout as e:
    raise SystemExit(f"listener did not rearm after accept error: {e}") from e
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"before-emfile"}
{"message":"after-emfile"}'
cmp_exact
exit_test
