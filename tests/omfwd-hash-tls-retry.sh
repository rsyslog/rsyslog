#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# pool.policy="hash": a deferred TLS send (RS_RET_RETRY) is repeated in place
# on the same target with the same bytes, within a bounded wait, so the batch
# is neither suspended nor replayed: every message arrives exactly once.
#
# liboverride_ossl_send_retry.so makes the first SSL_write of the sender
# defer, for OMFWD_HASH_RETRY_COUNT sends (default 1). The oracle is that the
# receiver gets every message exactly once (seq_check rejects gaps and
# duplicates) and that the fixture fired. With a count beyond the bounded
# wait (omfwd-hash-tls-retry-exhausted.sh) the action suspends and the engine
# replays the batch; the deferred target's share may then arrive twice, so
# only gaps are rejected.
#
# OMFWD_IOBUFFER selects where the deferral lands: unset, a batch of at most
# 8 frames fits the send buffer and the deferral hits the end-of-batch flush;
# a small value makes the buffer fill on a target's second frame, so it hits
# a send inside the batch (omfwd-hash-tls-retry-midbatch.sh). OMFWD_FRAME_PAD
# pads every frame past the 16 KiB send buffer, so frames bypass it and are
# written from a per-target copy (omfwd-hash-tls-retry-oversize.sh). OMFWD_FRAMING sets tcp_framing.
# Octet-counted framing makes tcpclt build every frame in a new buffer.
# OMFWD_HASH_RETRY_PARTIAL makes the deferred write follow a short write of
# that many bytes, so the repeat must continue after them
# (omfwd-hash-tls-retry-partial.sh, omfwd-hash-tls-retry-oversize-partial.sh).
. ${srcdir:=.}/diag.sh init
skip_platform "Darwin" "LD_PRELOAD fixtures are not loaded by dyld"
export NUMMESSAGES=${NUMMESSAGES:-2000}
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
RETRY_MARKER="$RSYSLOG_DYNNAME.retry-injected"

if [ -n "${OMFWD_FRAME_PAD:-}" ]; then
	PAD=$(printf "%${OMFWD_FRAME_PAD}s" "" | tr " " "x")
	RCV_MAXSIZE='global(maxMessageSize="64k")'
	SND_TEMPLATE='template(name="padded" type="string" string="<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag%%msg% '$PAD'\n")'
	SND_TEMPLATE_PARAM='template="padded"'
fi

generate_conf
add_conf '
'"${RCV_MAXSIZE:-}"'
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="ossl"
)
module(load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl" StreamDriver.Mode="1" StreamDriver.AuthMode="anon")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
# generate_conf 2 assigns a new TCPFLOOD_PORT, so both receiver ports are
# kept before it
PORT_RCVR1=$TCPFLOOD_PORT
PORT_RCVR2=$(cat "$RSYSLOG_DYNNAME.tcpflood_port2")

if [ -n "${OMFWD_IOBUFFER:-}" ]; then
	IOBUFFER_PARAM="iobuffer.maxsize=\"$OMFWD_IOBUFFER\""
fi
if [ -n "${OMFWD_FRAMING:-}" ]; then
	FRAMING_PARAM="tcp_framing=\"$OMFWD_FRAMING\""
fi
generate_conf 2
add_conf '
'"${RCV_MAXSIZE:-}"'
global(
	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="ossl"
)
# 8 frames stay far below the 16 KiB send buffer, so without OMFWD_IOBUFFER
# the first write of a batch is its end-of-batch flush
main_queue(queue.dequeueBatchSize="8")
module(load="builtin:omfwd" '"${IOBUFFER_PARAM:-}"')
template(name="shardkey" type="string" string="%msg:F,58:2%")
'"${SND_TEMPLATE:-}"'
:msg, contains, "msgnum:" action(type="omfwd" protocol="tcp"
	target=["127.0.0.1", "127.0.0.1"] port=["'$PORT_RCVR1'", "'$PORT_RCVR2'"]
	StreamDriverMode="1" StreamDriverAuthMode="anon"
	pool.policy="hash" pool.hashkey="shardkey" '"${SND_TEMPLATE_PARAM:-}"' '"${FRAMING_PARAM:-}"'
	action.resumeRetryCount="-1" action.resumeInterval="1")
:msg, contains, "dropping the connection" action(type="omfile" file="'$RSYSLOG_DYNNAME'.dropped.log")
' 2
export OVERRIDE_OSSL_SEND_RETRY_MARKER="$RETRY_MARKER"
export OVERRIDE_OSSL_SEND_RETRY_COUNT="${OMFWD_HASH_RETRY_COUNT:-1}"
export OVERRIDE_OSSL_SEND_RETRY_PARTIAL="${OMFWD_HASH_RETRY_PARTIAL:-0}"
export RSYSLOG_PRELOAD=.libs/liboverride_ossl_send_retry.so
startup 2
unset RSYSLOG_PRELOAD

injectmsg2
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

if [ ! -f "$RETRY_MARKER" ]; then
	echo "FAIL: the send retry was never injected, the test proved nothing"
	error_exit 1
fi
read -r owed partial < "$RETRY_MARKER"
if [ "${partial:-0}" -ne "${OMFWD_HASH_RETRY_PARTIAL:-0}" ]; then
	echo "FAIL: the deferral followed a short write of ${partial:-?} bytes, not ${OMFWD_HASH_RETRY_PARTIAL:-0}"
	error_exit 1
fi
if [ -n "${OMFWD_HASH_EXPECT_DROP:-}" ] && [ ! -s "$RSYSLOG_DYNNAME.dropped.log" ]; then
	echo "FAIL: the oversize deferral did not drop the connection"
	error_exit 1
fi
if [ -n "${OMFWD_FRAME_PAD:-}" ]; then
	owed=$(( ${owed:-0} + ${partial:-0} ))
	if [ "$owed" -le 16384 ]; then
		echo "FAIL: the deferred write was ${owed:-?} bytes, not a frame larger than the send buffer"
		error_exit 1
	fi
fi
if [ -n "${OMFWD_HASH_RETRY_DUPS_OK:-}" ]; then
	seq_check 0 $((NUMMESSAGES - 1)) -d
else
	seq_check
fi
exit_test
