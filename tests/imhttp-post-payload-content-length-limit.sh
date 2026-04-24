#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
check_command_available python3
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest" ruleset="ruleset")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup

response=$(python3 - "$IMHTTP_PORT" <<'PY'
import socket
import sys

port = int(sys.argv[1])
with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.settimeout(5)
    sock.sendall(
        b"POST /postrequest HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Content-Length: 999999999\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    data = sock.recv(4096)
print(data.decode("latin1", "replace").splitlines()[0] if data else "")
PY
)

echo "response: $response"
case "$response" in
	*" 413 "*) ;;
	*)
		echo "ERROR: oversized Content-Length was not rejected with 413"
		error_exit 1
		;;
esac

wait_queueempty
shutdown_when_empty
wait_shutdown
if [ -f "$RSYSLOG_OUT_LOG" ]; then
	content_count_check '999999999' 0
fi
exit_test
