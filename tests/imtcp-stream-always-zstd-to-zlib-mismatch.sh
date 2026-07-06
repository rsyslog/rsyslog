#!/bin/bash
# added 2026-05-17 by Codex, released under ASL 2.0
# Sends a zstd stream to a zlib-configured imtcp listener; the receiver must
# reject the compression-driver mismatch as an invalid compressed stream. The
# sender may keep retrying after the receiver closes the TCP session, so the
# oracle is the receiver diagnostic rather than sender queue drain.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime

generate_conf
add_conf '
global(processInternalMessages="on")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zlib")

if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
	stop
}
action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
'
startup
export RCVR_PORT=$TCPFLOOD_PORT

generate_conf 2
add_conf '
module(load="builtin:omfwd")

*.* action(type="omfwd"
	target="127.0.0.1"
	port="'$RCVR_PORT'"
	protocol="tcp"
	compression.mode="stream:always"
	compression.driver="zstd"
	compression.stream.flushOnTXEnd="on")
' 2
startup 2

injectmsg2 0 1
wait_content "imtcp: received invalid compressed stream" "$RSYSLOG_OUT_LOG"
shutdown_immediate 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

content_check "imtcp: received invalid compressed stream"
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	echo "FAIL: mismatched zstd-to-zlib stream unexpectedly produced messages"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
