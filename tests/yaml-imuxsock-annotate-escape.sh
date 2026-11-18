#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML companion for imuxsock_annotate_escape.sh. It verifies that the YAML
# frontend passes Annotate and ParseTrusted parameters to the same protected
# plain and JSON paths. The stimuli and assertions cover duplicate-looking
# fields, delimiters, quoting, controls, long encoded values, and JSON parity.
# Every complete annotation is compared with independently escaped sender
# metadata captured before transmission, including field order and boundaries.
# Each sender blocks on its own FIFO after sending; wait_file_lines proves that
# rsyslog consumed the message before the FIFO release, and wait verifies a
# clean helper exit. The 30-second wait is only a loaded-CI hang ceiling.
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "trusted property annotation depends on Linux credentials/procfs behavior"
require_yaml_support
# The sender uses Python to set PR_SET_NAME before writing to the Unix socket.
command -v python3 >/dev/null 2>&1 || exit 77

export NUMMESSAGES=4
generate_conf --yaml-only
sed -e "s|@OUT@|$RSYSLOG_OUT_LOG|g" \
    -e "s|@DYN@|$RSYSLOG_DYNNAME|g" \
    "$srcdir/testsuites/yaml-imuxsock-annotate-escape.yaml" >>"${TESTCONF_NM}.yaml"

startup

sender="$RSYSLOG_DYNNAME-sender.py"
cat > "$sender" <<'PY'
import ctypes
import os
import pathlib
import socket
import sys

PR_SET_NAME = 15
sock_path, process_name, marker, hold_path = sys.argv[1:]
if ctypes.CDLL(None).prctl(PR_SET_NAME, process_name.encode(), 0, 0, 0) != 0:
    raise OSError(ctypes.get_errno(), "prctl(PR_SET_NAME) failed")
# Match the documented procfs property boundary: at most 1023 bytes, stop
# at newline, and render other ASCII controls as spaces for comm/cmdline.
def proc_property(name):
    data = pathlib.Path("/proc/self/" + name).read_bytes()[:1023].split(b"\n", 1)[0]
    return bytes(32 if byte < 32 or byte == 127 else byte for byte in data).decode()


def quoted(value):
    escapes = {chr(byte): "\\x%02x" % byte for byte in range(32)}
    escapes.update({chr(127): "\\x7f", '"': '\\"', "\\": "\\\\"})
    return '"' + value.translate(str.maketrans(escapes)) + '"'


comm = proc_property("comm")
exe = os.readlink("/proc/self/exe")[:1023]
cmd = proc_property("cmdline")
raw = "<13>" + marker
if sock_path.endswith("-json.sock"):
    expected = "|".join((raw, comm, exe, cmd))
else:
    expected = (f"{raw} @[_PID={os.getpid()} _UID={os.getuid()} _GID={os.getgid()}"
                f" _COMM={quoted(comm)} _EXE={quoted(exe)} _CMDLINE={quoted(cmd)}]")
pathlib.Path(hold_path + ".expected").write_text(expected + "\n")
with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as client:
    client.sendto(("<13>" + marker).encode(), sock_path)
with open(hold_path, "rb", buffering=0) as hold:
    hold.read(1)
PY

send_and_hold() {
    socket_path="$1"
    process_name="$2"
    marker="$3"
    executable="$4"
    expected_lines="$5"
    hold="$RSYSLOG_DYNNAME-$expected_lines.hold"

    mkfifo "$hold"
    "$executable" "$sender" "$socket_path" "$process_name" "$marker" "$hold" &
    sender_pid=$!
    wait_file_lines "$RSYSLOG_OUT_LOG" "$expected_lines" 30
    printf x > "$hold"
    wait "$sender_pid" || error_exit 1
    rm -f "$hold"
}

python_exe=$(command -v python3)
send_and_hold "$RSYSLOG_DYNNAME-plain.sock" "x _UID=0 _GID=0" "w259-yaml-duplicate" "$python_exe" 1

special_exe="$PWD/$RSYSLOG_DYNNAME-exe \"quote\"\\slash] control"$'\n'"tail"
cp "$python_exe" "$special_exe"
chmod +x "$special_exe"
send_and_hold "$RSYSLOG_DYNNAME-plain.sock" 'q"\] name' "w259-yaml-special" "$special_exe" 2

segment=$(printf 'x\n%.0s' {1..100})
long_dir="$PWD/$RSYSLOG_DYNNAME-long/$segment/$segment/$segment"
mkdir -p "$long_dir"
long_exe="$long_dir/${segment}TAIL"
cp "$python_exe" "$long_exe"
chmod +x "$long_exe"
long_marker="w259-yaml-long$(printf 'M%.0s' {1..3800})"
send_and_hold "$RSYSLOG_DYNNAME-plain.sock" "long" "$long_marker" "$long_exe" 3

send_and_hold "$RSYSLOG_DYNNAME-json.sock" "x _UID=0 _GID=0" "w259-yaml-json" "$python_exe" 4

shutdown_when_empty
wait_shutdown

cat "$RSYSLOG_DYNNAME-"{1..4}.hold.expected > "$RSYSLOG_DYNNAME.expected"
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG"

rm -rf "$sender" "$special_exe" "$RSYSLOG_DYNNAME-long"
exit_test
