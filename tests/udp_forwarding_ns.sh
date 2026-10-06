#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify that omfwd creates its UDP socket in the configured network namespace.
# The receiver binds an ephemeral loopback port inside that namespace and records
# 100 datagrams. The complete 0..99 sequence proves delivery; the 30-second socket
# timeout only bounds a failed receiver so the test cannot hang indefinitely.
. ${srcdir:=.}/diag.sh init

if [ "$EUID" -ne 0 ]; then
	exit 77
fi
require_netns_capable

export NUMMESSAGES=100
NS_NAME="rsyslog_udp_$$"
PORT_FILE="${RSYSLOG_DYNNAME}.udp_ns.port"

cleanup_namespace() {
	if [ -n "${RECEIVER_PID:-}" ] && kill -0 "$RECEIVER_PID" 2>/dev/null; then
		kill "$RECEIVER_PID" 2>/dev/null || :
		wait "$RECEIVER_PID" 2>/dev/null || :
	fi
	ip netns delete "$NS_NAME" 2>/dev/null || :
}
trap cleanup_namespace EXIT

ip netns add "$NS_NAME"
ip netns exec "$NS_NAME" ip link set dev lo up

rm -f "$PORT_FILE" "$RSYSLOG_OUT_LOG"
ip netns exec "$NS_NAME" "$PYTHON" -u - "$PORT_FILE" "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" <<'PY' &
import socket
import sys

port_file, output_file, message_count = sys.argv[1], sys.argv[2], int(sys.argv[3])
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(30)
sock.bind(("127.0.0.1", 0))
with open(port_file, "w") as stream:
    stream.write("{0}\n".format(sock.getsockname()[1]))
with open(output_file, "ab", buffering=0) as stream:
    for _ in range(message_count):
        payload, _ = sock.recvfrom(65535)
        stream.write(payload)
PY
RECEIVER_PID=$!
wait_file_exists "$PORT_FILE"
PORT_RCVR="$(cat "$PORT_FILE")"

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfwd" template="outfmt" target="127.0.0.1"
	       port="'$PORT_RCVR'" protocol="udp" networknamespace="'$NS_NAME'")
'

startup
injectmsg 0 "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown

if ! wait "$RECEIVER_PID"; then
	RECEIVER_PID=""
	error_exit 1 "UDP namespace receiver failed"
fi
RECEIVER_PID=""

ip netns delete "$NS_NAME"
trap - EXIT
seq_check 0 99
exit_test
