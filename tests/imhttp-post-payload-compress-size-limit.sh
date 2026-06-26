#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify that imhttp applies the request body limit after gzip decompression,
# not only to the compressed wire body. The request uses octet-counted framing
# so a successful rejection cannot leave a partial message behind: the oracle is
# HTTP 413 plus an empty output file.

. ${srcdir:=.}/diag.sh init
check_command_available python3
generate_conf
IMHTTP_PORT_FILE="$RSYSLOG_DYNNAME.imhttp.port"
add_conf '
template(name="outfmt" type="string" string="%rawmsg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="0"
       listenPortFileName="'$IMHTTP_PORT_FILE'")
input(type="imhttp" endpoint="/postrequest"
      ruleset="ruleset"
      supportoctetcountedframing="on")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
assign_file_content IMHTTP_PORT "$IMHTTP_PORT_FILE"

response=$(python3 - "$IMHTTP_PORT" <<'PY'
import gzip
import socket
import sys

MAX_REQUEST_BODY_SIZE = 16 * 1024 * 1024
port = int(sys.argv[1])

payload = b"A" * (MAX_REQUEST_BODY_SIZE + 1)
body = str(len(payload)).encode("ascii") + b" " + payload
compressed = gzip.compress(body, compresslevel=9)
if len(compressed) >= MAX_REQUEST_BODY_SIZE:
    raise SystemExit("compressed reproducer body unexpectedly exceeds the wire-size limit")

request = b"".join(
    [
        b"POST /postrequest HTTP/1.1\r\n",
        b"Host: localhost\r\n",
        b"Content-Encoding: gzip\r\n",
        b"Content-Length: " + str(len(compressed)).encode("ascii") + b"\r\n",
        b"Connection: close\r\n",
        b"\r\n",
        compressed,
    ]
)

with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
    sock.settimeout(10)
    sock.sendall(request)
    response = b""
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        response += chunk

print(response.decode("latin1", "replace").splitlines()[0] if response else "")
PY
)

echo "response: $response"
case "$response" in
	*" 413 "*) ;;
	*)
		echo "ERROR: gzip-expanded request body was not rejected with 413"
		error_exit 1
		;;
esac

wait_queueempty
shutdown_when_empty
wait_shutdown
if [ -s "$RSYSLOG_OUT_LOG" ]; then
	echo "ERROR: gzip-expanded oversized request produced output"
	ls -l "$RSYSLOG_OUT_LOG"
	error_exit 1
fi
exit_test
