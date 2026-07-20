#!/bin/bash
# added 2026-07-13 by Codex, released under ASL 2.0
# Exercises the listener-wide zstd decoder-window budget with deliberately held
# frames. Diagnostics prove exact aggregate rejection and oversized-window
# rejection; delivered messages prove reservations are released after close,
# completion, and decode failure. A second unlimited listener holds three
# simultaneous windows and later completes them, proving that explicit zero
# does not impose a hidden cap. Polling allows ten seconds for loaded CI, while
# diagnostics and delivered messages—not elapsed time—are the oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime
check_command_available python3
export NUMMESSAGES=6
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.limited_port"
	compression.mode="stream:always"
	compression.driver="zstd"
	compression.maxTotalZstdWindowBytes="2097152")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.unlimited_port"
	compression.mode="stream:always"
	compression.driver="zstd"
	compression.maxTotalZstdWindowBytes="0")

if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
	stop
}
template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "window-budget-control" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

printf '<129>Mar 10 01:00:00 127.0.0.1 tag: window-budget-control\n' > "$RSYSLOG_DYNNAME.rawmsg"

python3 - "$RSYSLOG_DYNNAME.limited_port" "$RSYSLOG_DYNNAME.unlimited_port" "$RSYSLOG2_OUT_LOG" \
    "$RSYSLOG_OUT_LOG" "$RSYSLOG_DYNNAME.rawmsg" <<'PY'
import socket
import sys
import time


def read_port(path):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            with open(path, "r", encoding="ascii") as stream:
                return int(stream.read().strip())
        except (FileNotFoundError, ValueError):
            time.sleep(0.05)
    raise SystemExit(f"listener port was not published: {path}")


limited_port = read_port(sys.argv[1])
unlimited_port = read_port(sys.argv[2])
diagnostic_path = sys.argv[3]
output_path = sys.argv[4]
payload_path = sys.argv[5]


def wait_for(path, needle, deadline):
    while time.monotonic() < deadline:
        try:
            with open(path, "rb") as stream:
                if needle in stream.read():
                    return True
        except FileNotFoundError:
            pass
        time.sleep(0.05)
    return False


def wait_for_count(path, needle, count, deadline):
    while time.monotonic() < deadline:
        try:
            with open(path, "rb") as stream:
                if stream.read().count(needle) >= count:
                    return True
        except FileNotFoundError:
            pass
        time.sleep(0.05)
    return False


def connect(port):
    return socket.create_connection(("127.0.0.1", port))


# Standard zstd magic, a frame descriptor without a content size, and window
# descriptors for 1 MiB and 4 MiB. Omitting the block keeps the reservation live.
header_1m = bytes.fromhex("28b52ffd0050")
header_4m = bytes.fromhex("28b52ffd0060")

with open(payload_path, "rb") as stream:
    control = stream.read().rstrip(b"\n")


def raw_tail(payload):
    # A final raw block completes the frame without a custom compressor.
    body = payload + b"\n"
    return (1 | (len(body) << 3)).to_bytes(3, "little") + body


def complete_frame(payload):
    return header_1m + raw_tail(payload)


held_one = connect(limited_port)
held_one.sendall(header_1m)
held_two = connect(limited_port)
held_two.sendall(header_1m)

denied = connect(limited_port)
denied.sendall(header_1m)
if not wait_for(diagnostic_path, b"zstd decoder window budget exhausted", time.monotonic() + 10):
    raise SystemExit("listener admitted a third window beyond its 2 MiB budget")
denied.close()

oversized = connect(limited_port)
oversized.sendall(header_4m)
if not wait_for_count(
        diagnostic_path, b"zstd decoder window budget exhausted", 2, time.monotonic() + 10):
    raise SystemExit("listener did not reject a frame larger than its entire budget")
oversized.close()

# Closing one held session must make exactly one 1 MiB slot reusable. Split the
# replacement header across writes to exercise fragmented-header retention.
# TCP_NODELAY and the short gap let the server observe the incomplete prefix;
# retries below tolerate scheduler variation while preserving that oracle.
held_one.close()
first_completed = None
deadline = time.monotonic() + 10
while time.monotonic() < deadline:
    candidate = connect(limited_port)
    candidate.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    frame = complete_frame(control + b"-close-release")
    candidate.sendall(frame[:2])
    time.sleep(0.05)
    candidate.sendall(frame[2:])
    if wait_for(output_path, b"window-budget-control-close-release", time.monotonic() + 0.2):
        first_completed = candidate
        break
    candidate.close()
if first_completed is None:
    raise SystemExit("closing an incomplete frame did not release its reservation")

# Keep the completed TCP connection open. A new full-budget frame succeeds only
# if frame completion released the decoder context and reservation.
with connect(limited_port) as completed_again:
    completed_again.sendall(complete_frame(control + b"-completion-release"))
if not wait_for(output_path, b"window-budget-control-completion-release", time.monotonic() + 10):
    raise SystemExit("completed frame retained its decoder-window reservation")
first_completed.close()

# Release the remaining held slot, then force a decoder error after reservation.
# A subsequent valid frame proves that the error path returned the slot.
held_two.close()
corrupt = connect(limited_port)
corrupt.sendall(header_1m + bytes.fromhex("060000"))
corrupt.close()
error_release = None
deadline = time.monotonic() + 10
while time.monotonic() < deadline:
    candidate = connect(limited_port)
    candidate.sendall(complete_frame(control + b"-error-release"))
    if wait_for(output_path, b"window-budget-control-error-release", time.monotonic() + 0.2):
        error_release = candidate
        break
    candidate.close()
if error_release is None:
    raise SystemExit("decode failure did not release its decoder-window reservation")
error_release.close()

# Explicit zero must leave aggregate accounting disabled. Hold three 1 MiB
# headers simultaneously, then complete all three and require all messages.
unlimited = [connect(unlimited_port) for _ in range(3)]
for connection in unlimited:
    connection.sendall(header_1m)
for index, connection in enumerate(unlimited, start=1):
    connection.sendall(raw_tail(control + f"-unlimited-{index}".encode("ascii")))
for connection in unlimited:
    connection.close()
for index in range(1, 4):
    needle = f"window-budget-control-unlimited-{index}".encode("ascii")
    if not wait_for(output_path, needle, time.monotonic() + 10):
        raise SystemExit("explicit zero imposed an aggregate decoder-window cap")
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 6
shutdown_when_empty
wait_shutdown

content_check "zstd decoder window budget exhausted" "$RSYSLOG2_OUT_LOG"
content_check "window-budget-control-close-release" "$RSYSLOG_OUT_LOG"
content_check "window-budget-control-completion-release" "$RSYSLOG_OUT_LOG"
content_check "window-budget-control-error-release" "$RSYSLOG_OUT_LOG"
content_check "window-budget-control-unlimited-1" "$RSYSLOG_OUT_LOG"
content_check "window-budget-control-unlimited-2" "$RSYSLOG_OUT_LOG"
content_check "window-budget-control-unlimited-3" "$RSYSLOG_OUT_LOG"

exit_test
