#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# Regression test for imptcp's counted receive-buffer diagnostic. A recv()
# interposer puts a sentinel immediately after the returned two-byte frame; the
# debug record must report only those bytes and must not expose the sentinel.
# Clean message delivery plus normal shutdown are the compatibility oracle.
. ${srcdir:=.}/diag.sh init
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
require_plugin imptcp
check_command_available python3

export NUMMESSAGES=1
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"
export RSYSLOG_PRELOAD=.libs/liboverride_recv_imptcp.so
export RSYSLOG_TEST_IMPTCP_RECV_TAIL=1

generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%rawmsg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

python3 - "$TCPFLOOD_PORT" <<'PY'
import socket
import sys


with socket.create_connection(("127.0.0.1", int(sys.argv[1]))) as connection:
    connection.sendall(b"X\n")
PY

shutdown_when_empty
wait_shutdown
wait_file_lines
content_check "X"
content_check "imptcp: data(2)" "$RSYSLOG_DEBUGLOG"
check_not_present "IMPTCP_RECV_TAIL_SENTINEL" "$RSYSLOG_DEBUGLOG"
exit_test
