#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that plain imuxsock trusted properties quote and escape process names
# and executable paths, so field delimiters, quotes, backslashes, controls, and
# long encoded values cannot create ambiguous fields. A ParseTrusted JSON input
# is the compatibility control and must retain the original property values.
# Each sender blocks on its own FIFO after sending; wait_file_lines proves that
# rsyslog consumed the message before the FIFO release, and wait verifies a
# clean helper exit. The 30-second wait is only a loaded-CI hang ceiling.
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "trusted property annotation depends on Linux credentials/procfs behavior"
# The sender uses Python to set PR_SET_NAME before writing to the Unix socket.
command -v python3 >/dev/null 2>&1 || exit 77

export NUMMESSAGES=4
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")

template(name="plainfmt" type="string" string="%rawmsg%\n")
template(name="jsonfmt" type="string"
         string="%rawmsg%|%$!appname%|%$!exe%|%$!cmd%\n")

ruleset(name="plain") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="plainfmt")
}
ruleset(name="json") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="jsonfmt")
}

input(type="imuxsock"
      Socket="'$RSYSLOG_DYNNAME'-plain.sock"
      Annotate="on"
      ParseTrusted="off"
      Ruleset="plain")
input(type="imuxsock"
      Socket="'$RSYSLOG_DYNNAME'-json.sock"
      Annotate="on"
      ParseTrusted="on"
      Ruleset="json")
'

startup

sender="$RSYSLOG_DYNNAME-sender.py"
cat > "$sender" <<'PY'
import ctypes
import socket
import sys

PR_SET_NAME = 15
sock_path, process_name, marker, hold_path = sys.argv[1:]
if ctypes.CDLL(None).prctl(PR_SET_NAME, process_name.encode(), 0, 0, 0) != 0:
    raise OSError(ctypes.get_errno(), "prctl(PR_SET_NAME) failed")
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
send_and_hold "$RSYSLOG_DYNNAME-plain.sock" "x _UID=0 _GID=0" "w259-duplicate" "$python_exe" 1

special_exe="$PWD/$RSYSLOG_DYNNAME-exe \"quote\"\\slash] control"$'\n'"tail"
cp "$python_exe" "$special_exe"
chmod +x "$special_exe"
send_and_hold "$RSYSLOG_DYNNAME-plain.sock" 'q"\] name' "w259-special" "$special_exe" 2

segment=$(printf 'x\n%.0s' {1..100})
long_dir="$PWD/$RSYSLOG_DYNNAME-long/$segment/$segment/$segment"
mkdir -p "$long_dir"
long_exe="$long_dir/${segment}TAIL"
cp "$python_exe" "$long_exe"
chmod +x "$long_exe"
long_marker="w259-long$(printf 'M%.0s' {1..3800})"
send_and_hold "$RSYSLOG_DYNNAME-plain.sock" "long" "$long_marker" "$long_exe" 3

send_and_hold "$RSYSLOG_DYNNAME-json.sock" "x _UID=0 _GID=0" "w259-json" "$python_exe" 4

shutdown_when_empty
wait_shutdown

content_check '<13>w259-duplicate @['
content_check '_COMM="x _UID=0 _GID=0"'
check_not_present '_COMM=x _UID=0'
content_check '<13>w259-special @['
content_check '_COMM="q\"\\] name"'
content_check 'exe \"quote\"\\slash] control\x0atail" _CMDLINE='
content_check '<13>w259-long'
content_check 'TAIL" _CMDLINE='
content_check 'x\x0ax\x0ax'
content_check '<13>w259-json|x _UID=0 _GID=0|'

rm -rf "$sender" "$special_exe" "$RSYSLOG_DYNNAME-long"
exit_test
