#!/bin/bash
# Regression test for imtcp listener-descriptor teardown after concurrent TCP
# connection acceptance on Linux's epoll worker path. The client marks ready
# only after 128 sessions are established and holds those sessions open until
# shutdown completes. Passing requires the normal shutdown marker and no
# leftover pid file; sanitizer builds also turn a teardown UAF into a failure.
. ${srcdir:=.}/diag.sh init

if [ "$(uname)" != "Linux" ]; then
	echo "the tcpsrv worker-pool regression is specific to Linux epoll"
	exit 77
fi
check_command_available python3

generate_conf
add_conf '
$MaxOpenFiles 4096
module(load="../plugins/imtcp/.libs/imtcp" maxSessions="2048")
input(type="imtcp"
      address="127.0.0.1"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      socketBacklog="2048"
      workerThreads="8")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
rm -f "$RSYSLOG_DYNNAME.client-ready"
release_file="$RSYSLOG_DYNNAME.client-release"
rm -f "$release_file"

# Concurrent connects establish the workload before the shell requests
# shutdown. The release file is an explicit completion signal, replacing a
# host-speed-dependent delay while keeping established client sessions alive.
python3 - "$TCPFLOOD_PORT" "$RSYSLOG_DYNNAME.client-ready" "$release_file" <<'PY' &
import os
import socket
import sys
import threading
import time

port = int(sys.argv[1])
ready_file = sys.argv[2]
release_file = sys.argv[3]
sockets = []
lock = threading.Lock()
ready = threading.Event()
failed = threading.Event()


def connect_many():
    for _ in range(256):
        if failed.is_set() or ready.is_set():
            return
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=2)
        except OSError as error:
            with lock:
                if not ready.is_set() and not failed.is_set():
                    with open(ready_file, "w", encoding="ascii") as stream:
                        stream.write(f"error: {error}\n")
                    failed.set()
            return
        with lock:
            sockets.append(sock)
            if len(sockets) >= 128 and not ready.is_set() and not failed.is_set():
                with open(ready_file, "w", encoding="ascii") as stream:
                    stream.write("ready\n")
                ready.set()


threads = [threading.Thread(target=connect_many) for _ in range(8)]
for thread in threads:
    thread.start()
for thread in threads:
    thread.join()

if not ready.is_set():
    sys.exit(1)

# The shell normally creates the release file after shutdown. Do not leave this
# background helper holding sessions if the test harness terminates the shell.
release_deadline = time.monotonic() + 30
while not os.path.exists(release_file):
    if time.monotonic() >= release_deadline:
        raise SystemExit("timed out waiting for the test shutdown to complete")
    time.sleep(0.05)

for sock in sockets:
    try:
        sock.close()
    except OSError:
        pass
PY
client_pid=$!
trap 'touch "$release_file"; wait "$client_pid" 2>/dev/null || true' EXIT

wait_file_exists "$RSYSLOG_DYNNAME.client-ready"
if ! grep -qx "ready" "$RSYSLOG_DYNNAME.client-ready"; then
	cat "$RSYSLOG_DYNNAME.client-ready"
	error_exit 1 "connection setup failed before the shutdown workload was ready"
fi
shutdown_immediate
wait_shutdown "" 15
if [ -e "$RSYSLOG_PIDBASE.pid" ]; then
	error_exit 1 "rsyslogd left its pid file behind after shutdown"
fi
touch "$release_file"
wait "$client_pid" || error_exit 1
trap - EXIT

exit_test
