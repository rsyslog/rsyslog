#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify that omfwd UDP binds the longest-prefix source address from an array.
# The receiver chooses its port before publishing readiness and records the peer
# address plus ten datagrams. The 30-second timeout only bounds a failed helper;
# the observed 127.0.0.2 peer and complete 0..9 sequence are the test oracle.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10
PORT_FILE="${RSYSLOG_DYNNAME}.source_udp.port"
PEER_FILE="${RSYSLOG_DYNNAME}.source_udp.peer"

cleanup_receiver() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || :
        wait "$SERVER_PID" 2>/dev/null || :
    fi
}
trap cleanup_receiver EXIT

"$PYTHON" -u - "$PORT_FILE" "$PEER_FILE" "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" <<'PY' &
import socket
import sys

port_file, peer_file, output_file, message_count = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
receiver.settimeout(30)
receiver.bind(("127.0.0.2", 0))
with open(port_file, "w") as stream:
    stream.write("{0}\n".format(receiver.getsockname()[1]))
with open(output_file, "wb", buffering=0) as output:
    for index in range(message_count):
        data, peer = receiver.recvfrom(65535)
        if index == 0:
            with open(peer_file, "w") as stream:
                stream.write("{0}\n".format(peer[0]))
        output.write(data)
PY
SERVER_PID=$!
wait_file_exists "$PORT_FILE"

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
     action(type="omfwd" template="outfmt" target="127.0.0.2"
	       port="'"$(cat "$PORT_FILE")"'" protocol="udp"
         Address=["127.0.0.1/8", "127.0.0.2/32"])
'
startup
injectmsg 0 "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
wait "$SERVER_PID" || error_exit 1 "UDP source-address receiver failed"
SERVER_PID=""

content_check "127.0.0.2" "$PEER_FILE"
seq_check 0 9
trap - EXIT
exit_test
