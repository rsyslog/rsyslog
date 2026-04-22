#!/bin/bash
# added 2026-04-20 by Codex, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=8000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export STATSFILE="$RSYSLOG_DYNNAME.stats"

generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")

input(type="imptcp" name="streamstats" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
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
	compression.stream.flushOnTXEnd="on")
' 2
startup 2

injectmsg2
shutdown_when_empty 2
wait_shutdown 2
wait_file_lines
sleep 2
shutdown_when_empty
wait_shutdown

content_check "origin=imptcp" "$STATSFILE"
IMPTCP_STATS=$(grep 'origin=imptcp' "$STATSFILE")
MAX_RCV=$(printf '%s\n' "$IMPTCP_STATS" | grep -o 'bytes.received=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)
MAX_DECOMP=$(printf '%s\n' "$IMPTCP_STATS" | grep -o 'bytes.decompressed=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)
MAX_SUB=$(printf '%s\n' "$IMPTCP_STATS" | grep -o 'submitted=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)
MAX_OPENED=$(printf '%s\n' "$IMPTCP_STATS" | grep -o 'sessions.opened=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)
MAX_CLOSED=$(printf '%s\n' "$IMPTCP_STATS" | grep -o 'sessions.closed=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)

if [ -z "$MAX_RCV" ] || [ -z "$MAX_DECOMP" ] || [ -z "$MAX_SUB" ] || [ -z "$MAX_OPENED" ] || [ -z "$MAX_CLOSED" ]; then
	echo "FAIL: expected imptcp impstats counters missing"
	cat "$STATSFILE"
	error_exit 1
fi
if [ "$MAX_RCV" -le 0 ]; then
	echo "FAIL: expected bytes.received > 0, got $MAX_RCV"
	cat "$STATSFILE"
	error_exit 1
fi
if [ "$MAX_DECOMP" -le "$MAX_RCV" ]; then
	echo "FAIL: expected bytes.decompressed > bytes.received for stream compression, got $MAX_DECOMP <= $MAX_RCV"
	cat "$STATSFILE"
	error_exit 1
fi
if [ "$MAX_SUB" -lt "$NUMMESSAGES" ]; then
	echo "FAIL: expected submitted >= $NUMMESSAGES, got $MAX_SUB"
	cat "$STATSFILE"
	error_exit 1
fi
if [ "$MAX_OPENED" -lt 1 ] || [ "$MAX_CLOSED" -lt 1 ]; then
	echo "FAIL: expected imptcp session counters to record an open and close, got opened=$MAX_OPENED closed=$MAX_CLOSED"
	cat "$STATSFILE"
	error_exit 1
fi

seq_check
exit_test
