#!/bin/bash
# Native GnuTLS PQ smoke test. Requires a GnuTLS build with
# GROUP-X25519-MLKEM768 support.
. ${srcdir:=.}/diag.sh init

check_command_available gnutls-cli

export GNUTLS_PQ_PRIO='NORMAL:-GROUP-ALL:+GROUP-X25519-MLKEM768:+GROUP-X25519'
if ! gnutls-cli --priority "$GNUTLS_PQ_PRIO" --list >/dev/null 2>&1; then
	echo "SKIP: native GnuTLS PQ group GROUP-X25519-MLKEM768 is unavailable"
	skip_test
fi

export NUMMESSAGES=1000
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="gtls"
)

module(load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
	gnutlsPriorityString="'$GNUTLS_PQ_PRIO'")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.rcvr_port")

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'"
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
assign_file_content PORT_RCVR "$RSYSLOG_DYNNAME.rcvr_port"

export RSYSLOG_DEBUGLOG="log2"
generate_conf 2
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="gtls"
)

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(
	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	StreamDriver="gtls"
	StreamDriverMode="1"
	StreamDriverAuthMode="anon"
	gnutlsPriorityString="'$GNUTLS_PQ_PRIO'"
)
' 2
startup 2

tcpflood -m$NUMMESSAGES -i1
wait_file_lines
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

content_check --check-only "Syntax error or unsupported option in Priority String"
# shellcheck disable=SC2181
if [ $? -eq 0 ]; then
	echo "SKIP: native GnuTLS PQ group configuration was rejected"
	skip_test
fi

seq_check 1 $NUMMESSAGES
exit_test
