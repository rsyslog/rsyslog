#!/bin/bash
# added 2026-07-21 by Codex, released under ASL 2.0
# Verifies that imtcp releases a completed zlib inflater while its TCP socket
# remains open. The debug lifecycle marker proves inflateEnd() already ran;
# the delivered message and the diagnostic caused by a subsequent byte prove
# that same socket remained open after cleanup. Polling allows ten seconds for
# a loaded CI runner, while those state markers—not elapsed time—are the oracle.
. ${srcdir:=.}/diag.sh init
if [ "$(uname)" = "Darwin" ] && [[ "$CFLAGS" == *"sanitize=thread"* ]]; then
	echo "test skipped on macOS TSAN: lifecycle oracle requires debug logging, which exposes a known debug.c race"
	exit 77
fi
require_plugin imtcp
check_command_available python3
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"

generate_conf
add_conf '
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zlib")

template(name="outfmt" type="string" string="%msg%\n")
if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
	stop
}
if $msg contains "zlib-completion-release" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

python3 - "$TCPFLOOD_PORT" "$RSYSLOG_DEBUGLOG" "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" <<'PY'
import socket
import sys
import time
import zlib


port = int(sys.argv[1])
debug_path = sys.argv[2]
output_path = sys.argv[3]
diagnostic_path = sys.argv[4]
release_marker = b"released zlib decoder after stream completion"
message_marker = b"zlib-completion-release"
trailing_data_marker = b"data after end of zlib stream"


def wait_for(path, marker, deadline):
    while time.monotonic() < deadline:
        try:
            with open(path, "rb") as stream:
                if marker in stream.read():
                    return
        except FileNotFoundError:
            pass
        time.sleep(0.05)
    raise SystemExit(f"timed out waiting for {marker!r} in {path}")


payload = b"<129>Mar 10 01:00:00 127.0.0.1 tag: zlib-completion-release\n"
with socket.create_connection(("127.0.0.1", port)) as connection:
    connection.sendall(zlib.compress(payload))
    deadline = time.monotonic() + 10
    wait_for(debug_path, release_marker, deadline)
    wait_for(output_path, message_marker, deadline)
    connection.sendall(b"x")
    wait_for(diagnostic_path, trailing_data_marker, deadline)
PY

shutdown_when_empty
wait_shutdown
wait_file_lines
content_check "zlib-completion-release"
content_check "released zlib decoder after stream completion" "$RSYSLOG_DEBUGLOG"
content_check "data after end of zlib stream" "$RSYSLOG2_OUT_LOG"
exit_test
