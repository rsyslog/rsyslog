#!/bin/bash
# Verify YAML wiring and enforcement for all imbeats session deadlines. Three
# listeners isolate the invariants: idleTimeout closes an entirely idle peer,
# frameTimeout closes a partial body despite byte progress, and windowTimeout
# closes an incomplete advertised window despite receiving a complete event.
# Each listener has maxSessions=1. A peer close is the timeout oracle, and an
# ACKed valid client on that same listener proves its sole admission slot was
# reclaimed. The configured two-second limits are observed through bounded
# select/poll loops with a ten-second outer deadline, allowing loaded CI runners
# several timer sweeps without fixed sleeps or accepting mere silence as success.
. ${srcdir:=.}/diag.sh init
require_yaml_support

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imbeats/.libs/imbeats"'
add_yaml_conf ''
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg%\n"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imbeats'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.imbeats-idle.port\""
add_yaml_conf '    ruleset: main'
add_yaml_conf '    maxSessions: 1'
add_yaml_conf '    idleTimeout: 2'
add_yaml_conf '    frameTimeout: 0'
add_yaml_conf '  - type: imbeats'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.imbeats-frame.port\""
add_yaml_conf '    ruleset: main'
add_yaml_conf '    maxSessions: 1'
add_yaml_conf '    idleTimeout: 0'
add_yaml_conf '    frameTimeout: 2'
add_yaml_conf '  - type: imbeats'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.imbeats-window.port\""
add_yaml_conf '    ruleset: main'
add_yaml_conf '    maxSessions: 1'
add_yaml_conf '    idleTimeout: 0'
add_yaml_conf '    frameTimeout: 0'
add_yaml_conf '    windowTimeout: 2'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" file=\"${RSYSLOG_OUT_LOG}\" template=\"outfmt\")"

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats-idle.port"
IDLE_PORT="$RS_PORT"
assign_rs_port "$RSYSLOG_DYNNAME.imbeats-frame.port"
FRAME_PORT="$RS_PORT"
assign_rs_port "$RSYSLOG_DYNNAME.imbeats-window.port"
WINDOW_PORT="$RS_PORT"

if ! $PYTHON - "$IDLE_PORT" "$FRAME_PORT" "$WINDOW_PORT" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import select
import socket
import struct
import sys
import time

idle_port = int(sys.argv[1])
frame_port = int(sys.argv[2])
window_port = int(sys.argv[3])
expected_ack = b"2A" + struct.pack(">I", 1)


def event(message):
    payload = json.dumps({"message": message}, separators=(",", ":")).encode()
    return (
        b"2W" + struct.pack(">I", 1)
        + b"2J" + struct.pack(">I", 1)
        + struct.pack(">I", len(payload)) + payload
    )


def wait_for_close(sock, description):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        readable, _, _ = select.select([sock], [], [], min(0.25, deadline - time.monotonic()))
        if not readable:
            continue
        try:
            data = sock.recv(6)
        except (ConnectionResetError, BrokenPipeError):
            return
        if data == b"":
            return
        raise SystemExit(f"{description} unexpectedly received data: {data.hex()}")
    raise SystemExit(f"{description} was not closed within bounded timeout")


def recv_exactly(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if chunk == b"":
            break
        data += chunk
    return data


def wait_for_control(port, message):
    deadline = time.monotonic() + 10
    last = b""
    while time.monotonic() < deadline:
        with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
            sock.settimeout(1)
            sock.sendall(event(message))
            try:
                last = recv_exactly(sock, 6)
            except (ConnectionResetError, BrokenPipeError, socket.timeout):
                last = b""
            if last == expected_ack:
                return last
        select.select([], [], [], 0.05)
    raise SystemExit(f"valid client did not regain admission; last={last.hex()}")


idle = socket.create_connection(("127.0.0.1", idle_port), timeout=5)
try:
    wait_for_close(idle, "idle holder")
finally:
    idle.close()
idle_ack = wait_for_control(idle_port, "after-idle-timeout")

partial = socket.create_connection(("127.0.0.1", frame_port), timeout=5)
try:
    # A declared frame with a partial body starts frameTimeout while remaining
    # incomplete and therefore must never be submitted or ACKed.
    partial.sendall(
        b"2W" + struct.pack(">I", 1)
        + b"2J" + struct.pack(">I", 1)
        + struct.pack(">I", 128) + b"{"
    )
    wait_for_close(partial, "partial-body holder")
finally:
    partial.close()
frame_ack = wait_for_control(frame_port, "after-frame-timeout")

window = socket.create_connection(("127.0.0.1", window_port), timeout=5)
try:
    payload = json.dumps({"message": "must-not-submit"}, separators=(",", ":")).encode()
    # Complete one event in a two-event window. Byte and frame progress cannot
    # extend the absolute window deadline; the incomplete batch is never ACKed.
    window.sendall(
        b"2W" + struct.pack(">I", 2)
        + b"2J" + struct.pack(">I", 1)
        + struct.pack(">I", len(payload)) + payload
    )
    wait_for_close(window, "incomplete-window holder")
finally:
    window.close()
window_ack = wait_for_control(window_port, "after-window-timeout")

print(idle_ack.hex())
print(frame_ack.hex())
print(window_ack.hex())
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

EXPECTED='{"message":"after-idle-timeout"}
{"message":"after-frame-timeout"}
{"message":"after-window-timeout"}'
cmp_exact

EXPECTED_ACK='324100000001
324100000001
324100000001'
export EXPECTED="$EXPECTED_ACK"
cmp_exact "$RSYSLOG_DYNNAME.imbeats.ack"

exit_test
