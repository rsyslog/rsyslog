#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that imbeats explains why it closed a session and that these messages
# (and the too-many-sessions message) are rate limited per instance.
#
# Setup: rsyslog's own messages are written to a file via the default ruleset.
# Listener A uses maxWindowSize=4 and idleTimeout=2; listener B uses
# maxSessions=1.
# Stimulus and oracle (all read from the internal-message file after a
# synchronized shutdown):
#  1. 20 windows announcing 5 events (> maxWindowSize) on A, each closed by
#     the server without ACK. The file must contain the "window size 5 exceeds
#     maxWindowSize 4" message naming the peer 127.0.0.1 and A's port, and far
#     fewer than 20 of them (1 to 5: the limiter allows one per 10 seconds
#     per reason class, the slack tolerates a very slow runner crossing one
#     interval boundary).
#  2. One frame with an invalid frame type on A -> "invalid frame type" message
#     (a different reason class, so it is not hidden by the burst in step 1).
#  3. One connection on A that sends nothing; the server must close it after
#     idleTimeout (peer observes EOF within 15 s) and log "idle timeout".
#  4. With B's only session slot held open, 20 further connections are
#     refused; "too many sessions" must be logged at least once and at most 5
#     times (same reasoning as in step 1).
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%msg%\n")
template(name="internalfmt" type="string" string="%msg%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imbeats"
      port="0"
      name="closelogA"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats-a.port"
      ruleset="main"
      maxWindowSize="4"
      idleTimeout="2")

input(type="imbeats"
      port="0"
      name="closelogB"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats-b.port"
      ruleset="main"
      maxSessions="1")

# internal messages (iminternal) land in the default ruleset
action(type="omfile" file="'$RSYSLOG_DYNNAME'.internal.log" template="internalfmt")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats-a.port"
PORT_A="$RS_PORT"
assign_rs_port "$RSYSLOG_DYNNAME.imbeats-b.port"
PORT_B="$RS_PORT"

if ! $PYTHON - "$PORT_A" "$PORT_B" <<'PY'
import socket
import struct
import sys

port_a = int(sys.argv[1])
port_b = int(sys.argv[2])


def wait_closed(sock, what, limit=15):
    """Return once the server closed the connection; fail on data or timeout."""
    sock.settimeout(limit)
    try:
        data = sock.recv(6)
    except (ConnectionResetError, BrokenPipeError):
        return
    except (TimeoutError, socket.timeout):
        raise SystemExit(f"{what}: server did not close the connection")
    if data:
        raise SystemExit(f"{what}: unexpected data {data.hex()}")


def send_and_expect_close(port, wire, what):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        try:
            sock.sendall(wire)
        except (ConnectionResetError, BrokenPipeError):
            return
        wait_closed(sock, what)


# 1. oversized window announcements
for i in range(20):
    send_and_expect_close(port_a, b"2W" + struct.pack(">I", 5), f"oversized window {i}")

# 2. invalid frame type after a valid window header
send_and_expect_close(port_a, b"2W" + struct.pack(">I", 1) + b"2X", "invalid frame type")

# 3. idle connection, closed by idleTimeout=2
with socket.create_connection(("127.0.0.1", port_a), timeout=5) as idle:
    wait_closed(idle, "idle session")

# 4. max sessions: hold the only slot on B, then get refused repeatedly
holder = socket.create_connection(("127.0.0.1", port_b), timeout=5)
try:
    holder.sendall(b"2W" + struct.pack(">I", 1))  # partial window keeps the session open
    for i in range(20):
        try:
            extra = socket.create_connection(("127.0.0.1", port_b), timeout=5)
        except OSError:
            continue
        with extra:
            wait_closed(extra, f"refused session {i}")
finally:
    holder.close()
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

LOG="$RSYSLOG_DYNNAME.internal.log"

content_check "imbeats: closing session from 127.0.0.1:" "$LOG"
content_check "on port $PORT_A (input name 'closelogA'): window size 5 exceeds maxWindowSize 4" "$LOG"
content_check "on port $PORT_A (input name 'closelogA'): invalid frame type" "$LOG"
content_check "on port $PORT_A (input name 'closelogA'): idle timeout (idleTimeout 2 s)" "$LOG"
content_check "imbeats: too many sessions - dropping incoming request" "$LOG"

check_count() {
	local pattern="$1"
	local n
	n=$(grep -c -F -- "$pattern" "$LOG")
	if [ "$n" -lt 1 ] || [ "$n" -gt 5 ]; then
		echo "FAIL: expected 1..5 occurrences of '$pattern', got $n"
		cat "$LOG"
		error_exit 1
	fi
}
check_count "window size 5 exceeds maxWindowSize 4"
check_count "too many sessions - dropping incoming request"

exit_test
