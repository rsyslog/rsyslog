#!/bin/bash
# Regression test for imptcp compression.mode="stream:auto" when a plain
# octet-stuffed session starts with bytes that also look like a zlib header
# (ASCII "x^"). The oracle is that the line reaches omfile and a second
# line on the same connection is accepted; before the fix AUTO locked the
# session into the compressed path and closed it on zlib Z_DATA_ERROR.
#
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init

if ! command -v python3 >/dev/null 2>&1; then
	echo "SKIP: python3 is unavailable"
	skip_test
fi

generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      compression.mode="stream:auto")

*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port

python3 - <<'PY'
import os
import socket

port = int(os.environ["TCPFLOOD_PORT"])
payload = b"x^plain accepted by stream:auto\nsecond write after probe\n"
with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
    sock.sendall(payload)
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 2
shutdown_when_empty
wait_shutdown

content_check "x^plain accepted by stream:auto" "$RSYSLOG_OUT_LOG"
content_check "second write after probe" "$RSYSLOG_OUT_LOG"
exit_test
