#!/bin/bash
# Regression test for imptcp stream decompression with a decompressed payload
# that begins with the literal byte "z". Legacy single-message compression also
# uses a leading "z" in the built-in transport frame handling, but stream:always
# has already selected stream decompression and must not run legacy-z handling
# on the decoded bytes. The oracle is delivery of the raw z-prefixed message and
# absence of the legacy decompression-error diagnostic.
#
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available python3

generate_conf
add_conf '
global(processInternalMessages="on")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      compression.mode="stream:always")

template(name="rawline" type="string" string="%rawmsg%\n")

if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
	stop
}

*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="rawline")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

python3 - <<'PY'
import os
import socket
import zlib

port = int(os.environ["TCPFLOOD_PORT"])
payload = b"zstream payload starts z after imptcp inflate\n"
compressor = zlib.compressobj()
wire = compressor.compress(payload) + compressor.flush()

with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
    sock.sendall(wire)
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown

content_check "zstream payload starts z after imptcp inflate" "$RSYSLOG_OUT_LOG"
check_not_present "Uncompression of a message failed" "$RSYSLOG2_OUT_LOG"
check_not_present "imptcp: received invalid compressed stream" "$RSYSLOG2_OUT_LOG"
exit_test
