#!/bin/bash
# Regression test for imptcp compression.mode="stream:auto" after it classifies
# a connection as a zlib stream. A decompressed message can legitimately begin
# with literal "z"; once AUTO has selected the stream path via the zlib header,
# legacy-z transport-frame handling must not run on decoded bytes. The oracle is
# delivery of the raw z-prefixed message and no legacy decompression diagnostic.
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
      compression.mode="stream:auto")

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
payload = b"zauto stream payload starts z after imptcp inflate\n"
compressor = zlib.compressobj()
wire = compressor.compress(payload) + compressor.flush()

with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
    sock.sendall(wire)
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown

content_check "zauto stream payload starts z after imptcp inflate" "$RSYSLOG_OUT_LOG"
check_not_present "Uncompression of a message failed" "$RSYSLOG2_OUT_LOG"
check_not_present "imptcp: received invalid compressed stream" "$RSYSLOG2_OUT_LOG"
exit_test
