#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-09, released under ASL 2.0
# Verifies YAML parsing of the imptcp stream decompression limit parameters
# compression.maxExpansionRatio and compression.maxDecompressedBytesPerReceive
# (the RainerScript counterparts are imptcp-stream-always-zlib-*-guard.sh).
# The YAML defines two imptcp listeners. Listener A sets only a tiny expansion
# ratio ("2"); listener B disables the ratio ("0") and sets a tiny per-receive
# cap ("1000"). A ~270:1 zlib stream is sent to each listener. The oracle is
# that each listener logs its own limit as the reason for rejecting the stream
# (proving the YAML value reached the backend) and that no decoded message is
# submitted to the listeners' ruleset.
. ${srcdir:=.}/diag.sh init
require_plugin imptcp
check_command_available python3
wait_both_limits_logged() {
	content_check --check-only "decompressed bytes exceeded configured expansion ratio" "$RSYSLOG_OUT_LOG" &&
		content_check --check-only "decompressed bytes exceeded configured per-receive limit" "$RSYSLOG_OUT_LOG"
}
export QUEUE_EMPTY_CHECK_FUNC=wait_both_limits_logged

generate_conf
add_conf '
$MaxMessageSize 10k
global(processInternalMessages="on")

include(file="'${RSYSLOG_DYNNAME}'.yaml")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

cat > "${RSYSLOG_DYNNAME}.yaml" <<YAMLEOF
modules:
  - load: "../plugins/imptcp/.libs/imptcp"

inputs:
  - type: imptcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.ratio_port"
    compression.mode: "stream:always"
    compression.maxExpansionRatio: "2"
    ruleset: sink
  - type: imptcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.receive_port"
    compression.mode: "stream:always"
    compression.maxExpansionRatio: "0"
    compression.maxDecompressedBytesPerReceive: "1000"
    ruleset: sink

rulesets:
  - name: sink
    script: |
      action(type="omfile" file="${RSYSLOG2_OUT_LOG}")
YAMLEOF

startup
wait_file_exists "${RSYSLOG_DYNNAME}.ratio_port"
wait_file_exists "${RSYSLOG_DYNNAME}.receive_port"
PORT_RATIO=$(cat "${RSYSLOG_DYNNAME}.ratio_port")
PORT_RECEIVE=$(cat "${RSYSLOG_DYNNAME}.receive_port")

python3 - "$PORT_RATIO" "$PORT_RECEIVE" <<'PY'
import socket
import sys
import zlib

line = b"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
compressed = zlib.compress(line * 3000, level=9)
for port in sys.argv[1:]:
    sock = socket.create_connection(("127.0.0.1", int(port)))
    sock.sendall(compressed)
    sock.close()
PY

shutdown_when_empty
wait_shutdown

content_check "decompressed bytes exceeded configured expansion ratio"
content_check "decompressed bytes exceeded configured per-receive limit"
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: guarded zlib streams unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
