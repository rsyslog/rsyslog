#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Exercise long numeric PRI input through the real Unix socket receive path.
# An instrumented build also checks that accumulating PRI digits cannot
# overflow the integer before the syntax is rejected.
. "${srcdir:=.}/diag.sh" init
require_plugin imuxsock
check_command_available python3

export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf 'module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" socket="'"$RSYSLOG_DYNNAME"'-pri.sock" useSpecialParser="off")
template(name="outfmt" type="string" string="%rawmsg%\n")
action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")'
startup
python3 - "$RSYSLOG_DYNNAME-pri.sock" <<'PY'
import socket
import sys

with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as client:
    client.connect(sys.argv[1])
    client.send(("<" + "9" * 100 + ">guard: body").encode("ascii"))
PY
shutdown_when_empty
wait_shutdown
python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import pathlib
import sys

expected = "<" + "9" * 100 + ">guard: body\n"
actual = pathlib.Path(sys.argv[1]).read_text()
assert actual == expected, (actual, expected)
PY
exit_test
