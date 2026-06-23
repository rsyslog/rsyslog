#!/bin/bash
# Regression test for omuxsock action isolation. One Unix datagram receiver is
# intentionally left unread until the test releases it, which can block that
# action's send path. Success is proven by a second omuxsock action continuing
# to deliver messages to an independent receiver before the slow receiver is
# released. The fast receiver only needs to reach FAST_MIN_LINES because Unix
# datagram delivery can drop under burst pressure; the isolation invariant is
# that it still makes substantial progress while the slow action is blocked.
. ${srcdir:=.}/diag.sh init

if [ "$(uname)" != "Linux" ]; then
    echo "This test requires Linux AF_UNIX datagram socket behavior."
    exit 77
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "This test requires python3."
    exit 77
fi

FAST_SOCKET="${RSYSLOG_DYNNAME}-fast.sock"
SLOW_SOCKET="${RSYSLOG_DYNNAME}-slow.sock"
FAST_READY="${RSYSLOG_DYNNAME}.fast.ready"
SLOW_READY="${RSYSLOG_DYNNAME}.slow.ready"
SLOW_RELEASE="${RSYSLOG_DYNNAME}.slow.release"
NUMMESSAGES=5000
FAST_MIN_LINES=1000

test_error_exit_handler() {
    touch "$SLOW_RELEASE"
    if [ -n "${FAST_PID:-}" ]; then
        kill "$FAST_PID" 2>/dev/null || true
    fi
    if [ -n "${SLOW_PID:-}" ]; then
        kill "$SLOW_PID" 2>/dev/null || true
    fi
}

wait_ready_file() {
    ready_file="$1"
    count=1
    max_attempts=$((TB_TIMEOUT_STARTSTOP * 10))
    while [ "$count" -le "$max_attempts" ]; do
        if [ -r "$ready_file" ]; then
            return
        fi
        $TESTTOOL_DIR/msleep 100
        count=$((count + 1))
    done
    echo "receiver did not report ready file $ready_file"
    error_exit 1
}

python3 - "$SLOW_SOCKET" "$SLOW_READY" "$SLOW_RELEASE" <<'PY' &
import os
import socket
import sys
import time

path, ready, release = sys.argv[1:4]
try:
    os.unlink(path)
except FileNotFoundError:
    pass

sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
sock.bind(path)
with open(ready, "w", encoding="ascii") as fh:
    fh.write("ready\n")

while not os.path.exists(release):
    time.sleep(0.05)

sock.settimeout(0.05)
deadline = time.time() + 3
while time.time() < deadline:
    try:
        sock.recv(65535)
    except socket.timeout:
        pass
PY
SLOW_PID=$!
wait_ready_file "$SLOW_READY"

./uxsockrcvr -s"$FAST_SOCKET" -o "$RSYSLOG_OUT_LOG" -r "$FAST_READY" -t 60 &
FAST_PID=$!
wait_ready_file "$FAST_READY"

generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000

module(load="../plugins/omuxsock/.libs/omuxsock")
$template outfmt,"%msg:F,58:2%\n"

:msg, contains, "msgnum:" {
    action(
        type="omuxsock"
        SocketName="'$SLOW_SOCKET'"
        template="outfmt"
        queue.type="LinkedList"
        queue.timeoutEnqueue="0"
        queue.size="10000"
    )
    action(
        type="omuxsock"
        SocketName="'$FAST_SOCKET'"
        template="outfmt"
        queue.type="LinkedList"
        queue.timeoutEnqueue="0"
        queue.size="10000"
    )
}
'

startup
injectmsg 0 "$NUMMESSAGES"
wait_file_lines "$RSYSLOG_OUT_LOG" "$FAST_MIN_LINES" 20

touch "$SLOW_RELEASE"
wait_queueempty
shutdown_when_empty
wait_shutdown

kill "$FAST_PID"
wait "$FAST_PID"
wait "$SLOW_PID"

seq_check 0 $((NUMMESSAGES - 1)) -d -m$((NUMMESSAGES - FAST_MIN_LINES))
exit_test
