#!/bin/bash
# This file is part of rsyslog.
# Released under ASL 2.0.
# Verify that minitcpsrv clears poll revents when it recycles an accepted slot.
# A seed client closes after one record, proving the receiver compacted that
# slot. The next client stays connected without data; a later marker client
# must still be read. The marker arrives before the empty client is released,
# so its presence proves minitcpsrv waited for a fresh poll event rather than
# performing a stale POLLIN read on the empty connection. The wait is only a
# hang guard for that deterministic ordering.

. ${srcdir:=.}/diag.sh init
check_command_available python3
export TB_TEST_TIMEOUT

KEEP="$RSYSLOG_DYNNAME.minitcpsrv.keep"
ACCEPT_READY="$RSYSLOG_DYNNAME.minitcpsrv.accepted"
EMPTY_CONNECTED="$RSYSLOG_DYNNAME.minitcpsrv.empty-connected"
EMPTY_RELEASE="$RSYSLOG_DYNNAME.minitcpsrv.empty-release"

touch "$KEEP"
export MINITCPSRV_EXTRA_OPTS="-K $KEEP"
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1 "" "" "$ACCEPT_READY"
unset MINITCPSRV_EXTRA_OPTS

test_error_exit_handler() {
	: > "$EMPTY_RELEASE"
	if [ -n "${empty_client_pid:-}" ]; then
		wait "$empty_client_pid" 2>/dev/null || true
	fi
}

# Waiting for EOF proves the helper processed the seed peer's close and
# compacted its poll slot before the empty connection is created.
python3 - "$MINITCPSRVR_PORT1" <<'PY' || error_exit $?
import os
import socket
import sys

timeout = float(os.environ["TB_TEST_TIMEOUT"])
sock = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=timeout)
sock.sendall(b"seed\n")
sock.shutdown(socket.SHUT_WR)
while sock.recv(4096):
    pass
sock.close()
PY
wait_file_lines "$RSYSLOG_OUT_LOG" 1 "$TB_TEST_TIMEOUT"
content_check "seed" "$RSYSLOG_OUT_LOG"

rm -f "$ACCEPT_READY"
python3 - "$MINITCPSRVR_PORT1" "$EMPTY_CONNECTED" "$EMPTY_RELEASE" <<'PY' &
import os
import socket
import sys
import time

timeout = float(os.environ["TB_TEST_TIMEOUT"])
sock = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=timeout)
with open(sys.argv[2], "w") as marker:
    marker.write("connected\n")
while not os.path.exists(sys.argv[3]):
    time.sleep(0.01)
sock.close()
PY
empty_client_pid=$!
wait_file_exists "$EMPTY_CONNECTED"
wait_file_exists "$ACCEPT_READY"

python3 - "$MINITCPSRVR_PORT1" <<'PY' || error_exit $?
import os
import socket
import sys

timeout = float(os.environ["TB_TEST_TIMEOUT"])
sock = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=timeout)
sock.sendall(b"marker\n")
sock.shutdown(socket.SHUT_WR)
sock.close()
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 2 "$TB_TEST_TIMEOUT"
content_check "marker" "$RSYSLOG_OUT_LOG"

: > "$EMPTY_RELEASE"
wait "$empty_client_pid" || error_exit $?
empty_client_pid=''
rm -f "$KEEP"
stop_minitcpsrvrs
exit_test
