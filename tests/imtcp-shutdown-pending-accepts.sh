#!/bin/bash
# Regression test for listener-descriptor teardown racing imtcp workers.
. ${srcdir:=.}/diag.sh init

skip_platform "FreeBSD" "the tcpsrv worker-pool race is specific to epoll"
skip_platform "SunOS" "the tcpsrv worker-pool race is specific to epoll"
skip_platform "Darwin" "the tcpsrv worker-pool race is specific to epoll"
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

# Fill the listen backlog while a tcpsrv worker drains it. Reaching the marker
# proves that enough connects are pending to keep doAccept() active when the
# input thread starts shutdown. ASan turns the historical teardown race into a
# reliable UAF report; non-sanitizer jobs still check clean termination.
python3 - "$TCPFLOOD_PORT" "$RSYSLOG_DYNNAME.client-ready" <<'PY' &
import socket
import sys
import threading
import time

port = int(sys.argv[1])
ready_file = sys.argv[2]
sockets = []
lock = threading.Lock()
ready = threading.Event()


def connect_many():
    for _ in range(256):
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=2)
        except OSError:
            return
        with lock:
            sockets.append(sock)
            if len(sockets) >= 128 and not ready.is_set():
                with open(ready_file, "w", encoding="ascii") as stream:
                    stream.write("ready\n")
                ready.set()


threads = [threading.Thread(target=connect_many) for _ in range(8)]
for thread in threads:
    thread.start()
for thread in threads:
    thread.join()

time.sleep(1)
for sock in sockets:
    try:
        sock.close()
    except OSError:
        pass
PY
client_pid=$!

wait_file_exists "$RSYSLOG_DYNNAME.client-ready"
shutdown_immediate
wait_shutdown "" 15
if [ -e "$RSYSLOG_PIDBASE.pid" ]; then
	error_exit 1 "rsyslogd left its pid file behind after shutdown"
fi
wait "$client_pid" || error_exit 1

exit_test
