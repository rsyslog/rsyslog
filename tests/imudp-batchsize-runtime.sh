#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Exercise a non-default imudp BatchSize through the live recvmmsg receive
# path. The oracle waits for every locally sent datagram, verifies their exact
# sequence, and requires a clean shutdown; this proves the validated buffer
# layout is also used by the runtime iovec setup without relying on a delay.
. "${srcdir:=.}/diag.sh" init
require_plugin imudp

if ! grep -q '^#define HAVE_RECVMMSG 1' ../config.h; then
	echo "imudp BatchSize is intentionally ignored without recvmmsg()"
	exit 77
fi

export NUMMESSAGES=128
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.imudp_port"

generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp" BatchSize="128")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"
tcpflood -Tudp -p"$PORT_RCVR" -m"$NUMMESSAGES"
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" 100
shutdown_when_empty
wait_shutdown
seq_check 0 127
exit_test
