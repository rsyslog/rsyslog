#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Exercise long numeric PRI input through the real Unix socket receive path.
# An instrumented build also checks that accumulating PRI digits cannot
# overflow the integer before the syntax is rejected. A second input uses the
# default special parser; its exact TAG/body proves invalid PRI scanning still
# consumes every digit and preserves the established cursor position.
. "${srcdir:=.}/diag.sh" init
require_plugin imuxsock
check_command_available python3

export NUMMESSAGES=2
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf 'module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" socket="'"$RSYSLOG_DYNNAME"'-pri.sock" useSpecialParser="off")
input(type="imuxsock" socket="'"$RSYSLOG_DYNNAME"'-special.sock")
template(name="outfmt" type="string" string="%rawmsg%|%syslogtag%|%msg%\n")
if $inputname == "imuxsock" then action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")'
startup
python3 - "$RSYSLOG_DYNNAME-pri.sock" "$RSYSLOG_DYNNAME-special.sock" <<'PY' || error_exit 1
import socket
import sys

with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as client:
    client.connect(sys.argv[1])
    client.send(("<" + "9" * 100 + ">guard: body").encode("ascii"))
    client.sendto(b"<9999>guard: body", sys.argv[2])
PY
shutdown_when_empty
wait_shutdown
python3 - "$RSYSLOG_OUT_LOG" <<'PY' || error_exit 1
import pathlib
import sys

actual = pathlib.Path(sys.argv[1]).read_text().splitlines()
assert len(actual) == 2, actual
assert "<9999>guard: body|guard:| body" in actual, actual
long_lines = [line for line in actual if line.startswith("<" + "9" * 100 + ">guard: body|")]
assert len(long_lines) == 1, actual
PY
exit_test
