#!/bin/bash
# Regression test for empty output from the legacy per-message zlib extension.
# A nonempty TCP frame or UDP datagram can contain a valid zlib stream that
# expands to zero bytes. The parser must discard that record without aborting
# or reading before the message buffer. The oracle is daemon survival and
# delivery of a valid legacy-compressed follow-up on both transports; exactly
# those two successfully decompressed controls must reach the selected output.
# The debug log must contain exactly one boundary-specific discard for each
# transport. That exact count distinguishes the fixed decompression guard from
# the old release-build behavior, which could survive the invalid empty record
# and discard it only after an out-of-bounds sanitizer access. Daemon survival
# and control delivery are asserted independently through normal omfile output.
#
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin imudp
check_command_available python3
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debug.log"

TCP_PORT_FILE="$RSYSLOG_DYNNAME.tcp.port"
UDP_PORT_FILE="$RSYSLOG_DYNNAME.udp.port"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/imudp/.libs/imudp")

input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$TCP_PORT_FILE'" ruleset="legacy-zlib-input")
input(type="imudp" address="127.0.0.1" port="0"
      listenPortFileName="'$UDP_PORT_FILE'" ruleset="legacy-zlib-input")

template(name="rawline" type="string" string="%rawmsg%\n")
ruleset(name="legacy-zlib-input") {
	if $rawmsg contains "legacy-zlib-" then
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="rawline")
}
'
startup
wait_file_exists "$TCP_PORT_FILE"
wait_file_exists "$UDP_PORT_FILE"
export LEGACY_ZLIB_TCP_PORT LEGACY_ZLIB_UDP_PORT
LEGACY_ZLIB_TCP_PORT=$(cat "$TCP_PORT_FILE")
LEGACY_ZLIB_UDP_PORT=$(cat "$UDP_PORT_FILE")

python3 - <<'PY'
import os
import socket
import zlib


empty_record = b"z" + zlib.compress(b"")
tcp_control = b"z" + zlib.compress(
    b"<13>Oct 11 22:14:15 host tag: legacy-zlib-tcp-control")
udp_control = b"z" + zlib.compress(
    b"<13>Oct 11 22:14:15 host tag: legacy-zlib-udp-control")


def octet_frame(payload):
    return str(len(payload)).encode("ascii") + b" " + payload


with socket.create_connection(
        ("127.0.0.1", int(os.environ["LEGACY_ZLIB_TCP_PORT"])),
        timeout=10) as tcp_socket:
    tcp_socket.sendall(octet_frame(empty_record) + octet_frame(tcp_control))

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
    udp_socket.sendto(
        empty_record,
        ("127.0.0.1", int(os.environ["LEGACY_ZLIB_UDP_PORT"])))
    udp_socket.sendto(
        udp_control,
        ("127.0.0.1", int(os.environ["LEGACY_ZLIB_UDP_PORT"])))
PY

wait_file_lines "$RSYSLOG_OUT_LOG" 2
shutdown_when_empty
wait_shutdown

content_check "legacy-zlib-tcp-control" "$RSYSLOG_OUT_LOG"
content_check "legacy-zlib-udp-control" "$RSYSLOG_OUT_LOG"
content_count_check "legacy zlib decompression produced an empty message; discarding" 2 "$RSYSLOG_DEBUGLOG"
if [ "$(wc -l < "$RSYSLOG_OUT_LOG")" -ne 2 ]; then
	echo "FAIL: empty legacy zlib records produced unexpected output"
	exit 1
fi
exit_test
