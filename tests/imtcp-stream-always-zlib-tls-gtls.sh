#!/bin/bash
# added 2026-05-17 by Codex, released under ASL 2.0
# Verifies zlib stream:always decompression over a gtls imtcp listener fed by
# omfwd; the oracle is the complete ordered message sequence after TLS transit.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=1000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="gtls"
	StreamDriver.Mode="1" StreamDriver.AuthMode="anon")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zlib")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
export RCVR_PORT=$TCPFLOOD_PORT

generate_conf 2
add_conf '
global(defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'")

action(	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$RCVR_PORT'"
	StreamDriver="gtls"
	StreamDriverMode="1"
	StreamDriverAuthMode="x509/certvalid"
	compression.mode="stream:always"
	compression.driver="zlib"
	compression.stream.flushOnTXEnd="on"
	)
' 2
startup 2

injectmsg2
wait_file_lines
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

seq_check
exit_test
