#!/bin/bash
# Regression test for the global legacy-z decompression opt-out on imptcp input.
# compression.mode="none" disables stream decompression only; legacy single
# message compression is intentionally still built-in transport compatibility.
# The oracle is that parser.supportCompressionExtension="off" delivers a plain
# raw frame beginning with "z" unchanged and emits no legacy decompression
# diagnostic. pmnull is used only so the test can assert the raw payload without
# message parsing changes; it does not control the legacy-z decision.
#
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available python3

generate_conf
add_conf '
global(processInternalMessages="on" parser.supportCompressionExtension="off")

module(load="../plugins/imptcp/.libs/imptcp")
module(load="../plugins/pmnull/.libs/pmnull")

input(type="imptcp"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      compression.mode="none"
      ruleset="plain_pmnull")

template(name="rawline" type="string" string="%rawmsg%\n")

if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
	stop
}

ruleset(name="plain_pmnull" parser="rsyslog.pmnull") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="rawline")
}
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

python3 - <<'PY'
import os
import socket

port = int(os.environ["TCPFLOOD_PORT"])
with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
    sock.sendall(b"zis plain text, not a compressed stream\n")
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown

content_check "zis plain text, not a compressed stream" "$RSYSLOG_OUT_LOG"
check_not_present "Uncompression of a message failed" "$RSYSLOG2_OUT_LOG"
check_not_present "imptcp: received invalid compressed stream" "$RSYSLOG2_OUT_LOG"
exit_test
