#!/bin/bash
# Reproduces the imtcp accept-error throttle: doSingleAccept() in
# runtime/tcpsrv.c sleeps 20ms on EVERY accept error. Ghost clients (TCP
# connect + immediate close) make the TLS handshake fail in the accept
# path, pinning the listener at ~50 accepts/sec. gtls anon authmode needs
# no certificate files. MaxSessions="20000" keeps the derived backlog
# large enough that all connects queue instantly, so the drain time is
# server-bound.
#
# Oracle: count "failed to process incoming connection" lines in the
# harness-captured .started file until GHOSTS is reached; the drain must
# finish in < 1s. With the bug 100 failures take >= 2s (100 x 20ms);
# without it, well under 300ms. The 1s threshold has ~10x margin on both
# sides. internalmsg.ratelimit.burst is raised because the default
# (100 per 60s) would discard the failure messages the oracle counts.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(	internalmsg.ratelimit.burst="10000"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
	MaxSessions="20000"
	)
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
'
STARTED_LOG="${RSYSLOG_DYNNAME}.started"
GHOSTS=100
LOAD_DURATION=10
DRAIN_TIMEOUT=30

if ! command -v python3 >/dev/null 2>&1; then
	echo "SKIP: python3 is required for the ghost-client load generator"
	exit 77
fi

startup
assign_file_content TCPFLOOD_PORT "$RSYSLOG_DYNNAME.tcpflood_port"

count_failures() {
	local n
	n=$(grep -c 'failed to process incoming connection' "$STARTED_LOG" 2>/dev/null)
	echo "${n:-0}"
}

# Ghost-client load: 200 threads doing connect + close in a tight loop.
# Continuous concurrent load is required to expose the throttle: a failed
# accept aborts the backlog drain (bRun = 0), so the EPOLLONESHOT
# listener only advances on new incoming connections.
python3 - "$TCPFLOOD_PORT" "$LOAD_DURATION" <<'EOF' &
import socket, sys, threading, time
port = int(sys.argv[1])
stop = time.time() + int(sys.argv[2])
def ghost():
    while time.time() < stop:
        try:
            s = socket.socket(); s.settimeout(2)
            s.connect(('127.0.0.1', port)); s.close()
        except Exception:
            time.sleep(0.01)
for _ in range(200):
    threading.Thread(target=ghost, daemon=True).start()
time.sleep(int(sys.argv[2]))
EOF

cleanup_ghosts() {
	[ -n "${GHOST_PID:-}" ] || return 0
	kill "$GHOST_PID" 2>/dev/null
	wait "$GHOST_PID" 2>/dev/null
}

GHOST_PID=$!

T0=$(date +%s%N)
deadline=$(( $(date +%s) + DRAIN_TIMEOUT ))
while [ "$(count_failures)" -lt "$GHOSTS" ]; do
	if [ "$(date +%s)" -ge "$deadline" ]; then
		cleanup_ghosts
		echo "FAIL: only $(count_failures) of $GHOSTS ghost connections \
were processed within ${DRAIN_TIMEOUT}s"
		error_exit 1
	fi
	sleep 0.1
done
T1=$(date +%s%N)
cleanup_ghosts

ELAPSED_MS=$(( (T1 - T0) / 1000000 ))
echo "drained $GHOSTS failed accepts in ${ELAPSED_MS}ms"
if [ "$ELAPSED_MS" -gt 0 ]; then
	echo "accept rate: $(( GHOSTS * 1000 / ELAPSED_MS ))/sec (bug ceiling: ~50/sec)"
else
	echo "accept rate: >$(( GHOSTS * 1000 ))/sec (bug ceiling: ~50/sec)"
fi

if [ "$ELAPSED_MS" -ge 1000 ]; then
	echo "FAIL: accept-error path took ${ELAPSED_MS}ms for $GHOSTS peers \
(>= 2000ms expected with the 20ms srSleep bug)"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
