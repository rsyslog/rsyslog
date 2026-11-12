#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify YAML source-address arrays bind omfwd TCP to the longest-prefix entry.
# The listener publishes an ephemeral port after listen succeeds and records the
# peer address. Observing 127.0.0.2 and message zero is the test oracle.
. ${srcdir:=.}/diag.sh init
require_yaml_support

PORT_FILE="${RSYSLOG_DYNNAME}.yaml_source.port"
PEER_FILE="${RSYSLOG_DYNNAME}.yaml_source.peer"

cleanup_receiver() {
  if [ -n "${SERVER_PID:-}" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null || :
    wait "$SERVER_PID" 2>/dev/null || :
  fi
}
trap cleanup_receiver EXIT

"$PYTHON" -u - "$PORT_FILE" "$PEER_FILE" "$RSYSLOG_OUT_LOG" <<'PY' &
import socket
import sys

port_file, peer_file, output_file = sys.argv[1:]
listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listener.bind(("127.0.0.2", 0))
listener.listen(1)
listener.settimeout(30)
with open(port_file, "w") as stream:
    stream.write("{0}\n".format(listener.getsockname()[1]))
connection, peer = listener.accept()
with open(peer_file, "w") as stream:
    stream.write("{0}\n".format(peer[0]))
with open(output_file, "wb") as stream:
    while True:
        data = connection.recv(65535)
        if not data:
            break
        stream.write(data)
PY
SERVER_PID=$!
wait_file_exists "$PORT_FILE"

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
call main
'
cat > "${RSYSLOG_DYNNAME}.yaml" <<YAMLEOF
templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\\n"
rulesets:
  - name: main
    filter: ':msg, contains, "msgnum:"'
    actions:
      - type: omfwd
        template: outfmt
        target: 127.0.0.2
        port: "$(cat "$PORT_FILE")"
        protocol: tcp
        Address: ["127.0.0.1/8", "127.0.0.2/32"]
YAMLEOF

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
wait "$SERVER_PID" || error_exit 1 "YAML source-address receiver failed"
SERVER_PID=""

content_check "127.0.0.2" "$PEER_FILE"
seq_check 0 0
trap - EXIT
exit_test
