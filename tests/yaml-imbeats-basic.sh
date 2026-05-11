#!/bin/bash
. ${srcdir:=.}/diag.sh init
require_plugin imbeats

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imbeats/.libs/imbeats"'
add_yaml_conf ''
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg%|%$!message%|%$!metadata!imbeats!sequence%\n"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_imdiag_input
add_yaml_conf '  - type: imbeats'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.imbeats.port\""
add_yaml_conf '    ruleset: main'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" file=\"${RSYSLOG_OUT_LOG}\" template=\"outfmt\")"

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

python3 - "$RS_PORT" <<'PY'
import json
import socket
import struct
import sys

port = int(sys.argv[1])
payload = json.dumps({"message": "yaml"}, separators=(",", ":")).encode()
wire = b"2W" + struct.pack(">I", 1) + b"2J" + struct.pack(">I", 1) + struct.pack(">I", len(payload)) + payload

sock = socket.create_connection(("127.0.0.1", port))
sock.sendall(wire)
sock.recv(6)
sock.close()
PY

shutdown_when_empty
wait_shutdown

EXPECTED='{"message":"yaml"}|yaml|1'
cmp_exact

exit_test
