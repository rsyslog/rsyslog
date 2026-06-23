#!/bin/bash
# added 2026-05-17 by Codex, released under ASL 2.0
# Verifies imtcp stream:always accounting for zlib traffic; compressed bytes
# must be positive and decompressed bytes must exceed wire bytes.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin impstats
export NUMMESSAGES=8000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export STATSFILE="$RSYSLOG_DYNNAME.stats"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")

input(type="imtcp" name="streamstats" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zlib")

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
	compression.driver="zlib"
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

content_check "origin=imtcp" "$STATSFILE"
IMTCP_STATS=$(grep 'origin=imtcp' "$STATSFILE")
MAX_RCV=$(printf '%s\n' "$IMTCP_STATS" | grep -o 'bytes.received=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)
MAX_DECOMP=$(printf '%s\n' "$IMTCP_STATS" | grep -o 'bytes.decompressed=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)
MAX_SUB=$(printf '%s\n' "$IMTCP_STATS" | grep -o 'submitted=[0-9][0-9]*' | cut -d= -f2 | sort -n | tail -n1)

if [ -z "$MAX_RCV" ] || [ -z "$MAX_DECOMP" ] || [ -z "$MAX_SUB" ]; then
	echo "FAIL: expected imtcp impstats counters missing"
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

seq_check
exit_test
