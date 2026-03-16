#!/bin/bash
# Native OpenSSL PQ smoke test. Requires an OpenSSL build with
# X25519MLKEM768 support.
. ${srcdir:=.}/diag.sh init

check_command_available openssl

if ! openssl list -tls-groups 2>/dev/null | tr ': ,' '\n' | grep -q '^X25519MLKEM768$'; then
	echo "SKIP: native OpenSSL PQ group X25519MLKEM768 is unavailable"
	skip_test
fi

export NUMMESSAGES=1000
export RSYSLOG_DEBUGLOG="log"
export OSSL_PQ_CFG='MinProtocol=TLSv1.3\nMaxProtocol=TLSv1.3\nGroups=X25519MLKEM768'
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module(load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
	gnutlsPriorityString="'$OSSL_PQ_CFG'")

input(type="imtcp" port="'$PORT_RCVR'")

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'"
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup

export RSYSLOG_DEBUGLOG="log2"
generate_conf 2
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(
	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	StreamDriver="ossl"
	StreamDriverMode="1"
	StreamDriverAuthMode="anon"
	gnutlsPriorityString="'$OSSL_PQ_CFG'"
)
' 2
startup 2

tcpflood -m$NUMMESSAGES -i1
wait_file_lines
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

content_check --check-only "Failed to add OpenSSL command"
# shellcheck disable=SC2181
if [ $? -eq 0 ]; then
	echo "SKIP: native OpenSSL PQ group configuration was rejected"
	skip_test
fi

seq_check 1 $NUMMESSAGES
exit_test
