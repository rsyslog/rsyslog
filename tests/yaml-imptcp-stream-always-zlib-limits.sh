#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-09 by Rainer Gerhards, released under ASL 2.0
# Verifies YAML parsing of the imptcp stream decompression limit parameters
# compression.maxExpansionRatio and compression.maxDecompressedBytesPerReceive
# (the RainerScript counterparts are imptcp-stream-always-zlib-*-guard.sh).
# The YAML defines three imptcp listeners. Listener A sets only a tiny expansion
# ratio ("2"); listener B disables the ratio ("0") and sets a tiny per-receive
# cap ("1000"). A ~270:1 zlib stream is sent to each of them. The oracle is
# that each listener logs its own limit as the reason for rejecting the stream
# (proving the YAML value reached the backend) and that no decoded message is
# submitted to the listeners' ruleset.
# Listener C only disables the ratio ("0") and receives a ~1027:1 stream (empty
# lines, which imptcp discards, followed by one marker message). That ratio is
# above the 1024:1 default, so the marker is only delivered (and the expansion
# ratio error logged just once, for listener A) if "0" really disables the
# guard.
. ${srcdir:=.}/diag.sh init
require_plugin imptcp
check_command_available python3
wait_both_limits_logged() {
	content_check --check-only "decompressed bytes exceeded configured expansion ratio" "$RSYSLOG_OUT_LOG" &&
		content_check --check-only "decompressed bytes exceeded configured per-receive limit" "$RSYSLOG_OUT_LOG" &&
		content_check --check-only "msgnum:ratio-disabled" "$RSYSLOG_DYNNAME.ratiooff.log"
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
  - type: imptcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.ratiooff_port"
    compression.mode: "stream:always"
    compression.maxExpansionRatio: "0"
    ruleset: ratiooff

rulesets:
  - name: sink
    script: |
      action(type="omfile" file="${RSYSLOG2_OUT_LOG}")
  - name: ratiooff
    script: |
      action(type="omfile" file="${RSYSLOG_DYNNAME}.ratiooff.log")
YAMLEOF

startup
wait_file_exists "${RSYSLOG_DYNNAME}.ratio_port"
wait_file_exists "${RSYSLOG_DYNNAME}.receive_port"
wait_file_exists "${RSYSLOG_DYNNAME}.ratiooff_port"
PORT_RATIO=$(cat "${RSYSLOG_DYNNAME}.ratio_port")
PORT_RECEIVE=$(cat "${RSYSLOG_DYNNAME}.receive_port")
PORT_RATIOOFF=$(cat "${RSYSLOG_DYNNAME}.ratiooff_port")

python3 - "$PORT_RATIO" "$PORT_RECEIVE" "$PORT_RATIOOFF" <<'PY'
import socket
import sys
import zlib

line = b"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
compressed = zlib.compress(line * 3000, level=9)
for port in sys.argv[1:3]:
    sock = socket.create_connection(("127.0.0.1", int(port)))
    sock.sendall(compressed)
    sock.close()
# ~1027:1, above the 1024:1 default ratio limit, below the per-receive default
marker = b"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:ratio-disabled\n"
compressed = zlib.compress(b"\n" * 50000000 + marker, level=9)
sock = socket.create_connection(("127.0.0.1", int(sys.argv[3])))
sock.sendall(compressed)
sock.close()
PY

shutdown_when_empty
wait_shutdown

content_check "decompressed bytes exceeded configured expansion ratio"
content_check "decompressed bytes exceeded configured per-receive limit"
content_check "msgnum:ratio-disabled" "$RSYSLOG_DYNNAME.ratiooff.log"
if [ "$(grep -c "decompressed bytes exceeded configured expansion ratio" "$RSYSLOG_OUT_LOG")" != 1 ]; then
	echo "FAIL: expansion ratio limit was not disabled by compression.maxExpansionRatio=\"0\""
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: guarded zlib streams unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
