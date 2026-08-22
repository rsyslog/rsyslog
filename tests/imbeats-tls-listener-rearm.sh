#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify a malformed plaintext connection does not disarm an imbeats TLS
# listener. The plaintext peer waits for its close to prove the failed accept
# completed; a subsequent TLS Lumberjack event must receive its ACK and reach
# the output file. The listener may reject the malformed handshake before the
# peer can shut down its write side, so that expected socket error is tolerated
# before observing the close. Socket timeouts only bound a listener that
# remains disarmed.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf "
global(
	defaultNetstreamDriverCAFile=\"$srcdir/tls-certs/ca.pem\"
	defaultNetstreamDriverCertFile=\"$srcdir/tls-certs/cert.pem\"
	defaultNetstreamDriverKeyFile=\"$srcdir/tls-certs/key.pem\"
)
"

add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imbeats"
      address="127.0.0.1"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main"
      streamDriver.name="ossl"
      streamDriver.mode="1"
      streamDriver.authMode="anon")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" <<'PY'
import errno
import json
import socket
import ssl
import struct
import sys

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=5) as plain:
    plain.settimeout(5)
    plain.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    try:
        plain.shutdown(socket.SHUT_WR)
    except OSError as e:
        # The listener may already have closed after rejecting plaintext.
        if e.errno != errno.ENOTCONN:
            raise
    try:
        response = plain.recv(1)
    except socket.timeout as e:
        raise SystemExit(f"plaintext peer timed out waiting for close: {e}") from e
    except (ConnectionResetError, OSError):
        pass
    else:
        if response:
            raise SystemExit(f"unexpected response to plaintext trigger: {response!r}")

payload = json.dumps({"message": "after-plaintext"}, separators=(",", ":")).encode()
wire = b"2W" + struct.pack(">I", 1)
wire += b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(payload)) + payload

context = ssl._create_unverified_context()


def recv_exact(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise SystemExit("TLS listener closed before its Lumberjack ACK")
        data += chunk
    return data


with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
    raw.settimeout(5)
    with context.wrap_socket(raw, server_hostname="localhost") as tls:
        tls.sendall(wire)
        ack = recv_exact(tls, 6)
        if ack != b"2A" + struct.pack(">I", 1):
            raise SystemExit(f"unexpected ack after plaintext trigger: {ack.hex()}")
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED='{"message":"after-plaintext"}'
cmp_exact

exit_test
