#!/bin/bash
# added 2026-07-13 by Codex, released under ASL 2.0
# Holds an incomplete zstd frame that consumes the listener's configured 1 MiB
# decoder-window budget, then proves a second session is rejected. It also
# keeps a completed connection open while proving its reservation is reusable.
# The first complete frame sends its header in two writes separated by 50 ms so
# the receive path must retain partial header bytes. Polling has a ten-second CI
# tolerance; diagnostics and delivered messages, not elapsed time, are the
# oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime
check_command_available python3
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zstd"
	compression.maxDecompressedBytesPerReceive="1048576")

if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
	stop
}
template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "window-budget-control" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

printf '<129>Mar 10 01:00:00 127.0.0.1 tag: window-budget-control\n' > "$RSYSLOG_DYNNAME.rawmsg"

python3 - "$TCPFLOOD_PORT" "$RSYSLOG2_OUT_LOG" "$RSYSLOG_OUT_LOG" "$RSYSLOG_DYNNAME.rawmsg" <<'PY'
import socket
import sys
import time

port = int(sys.argv[1])
diagnostic_path = sys.argv[2]
output_path = sys.argv[3]
payload_path = sys.argv[4]


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


# Standard zstd magic, a frame descriptor without a content size, and a window
# descriptor for windowLog=20 (1 MiB). The absent block keeps the frame live.
incomplete_1m_header = bytes.fromhex("28b52ffd0050")
held = socket.create_connection(("127.0.0.1", port))
held.sendall(incomplete_1m_header)

denied = socket.create_connection(("127.0.0.1", port))
denied.sendall(incomplete_1m_header)
if not wait_for(diagnostic_path, b"zstd decoder window budget exhausted", time.monotonic() + 10):
    raise SystemExit("listener did not reject a second 1 MiB decoder window")
denied.close()
held.close()

with open(payload_path, "rb") as stream:
    control = stream.read()


def raw_1m_frame(payload):
    # A final raw block keeps the advertised 1 MiB window while producing a
    # complete, standards-compliant frame without needing a custom compressor.
    block_header = (1 | (len(payload) << 3)).to_bytes(3, "little")
    return incomplete_1m_header + block_header + payload


first_payload = control.rstrip(b"\n") + b"-first\n"
first_completed = None
deadline = time.monotonic() + 10
while time.monotonic() < deadline:
    candidate = socket.create_connection(("127.0.0.1", port))
    candidate.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    first_frame = raw_1m_frame(first_payload)
    candidate.sendall(first_frame[:2])
    # Give the server a chance to consume the deliberately incomplete header;
    # message delivery below remains the pass/fail oracle, not this delay.
    time.sleep(0.05)
    candidate.sendall(first_frame[2:])
    if wait_for(output_path, b"window-budget-control-first", time.monotonic() + 0.2):
        first_completed = candidate
        break
    candidate.close()
if first_completed is None:
    raise SystemExit("listener did not accept the first complete 1 MiB-window frame")

# Keep the first TCP connection open. The second full-budget frame can only
# succeed if completion freed the first decoder context and its reservation.
second_payload = control.rstrip(b"\n") + b"-second\n"
with socket.create_connection(("127.0.0.1", port)) as second_completed:
    second_completed.sendall(raw_1m_frame(second_payload))
if not wait_for(output_path, b"window-budget-control-second", time.monotonic() + 10):
    raise SystemExit("completed stream retained its decoder-window reservation")
first_completed.close()
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 2
shutdown_when_empty
wait_shutdown

content_check "zstd decoder window budget exhausted" "$RSYSLOG2_OUT_LOG"
content_check "window-budget-control-first" "$RSYSLOG_OUT_LOG"
content_check "window-budget-control-second" "$RSYSLOG_OUT_LOG"

exit_test
