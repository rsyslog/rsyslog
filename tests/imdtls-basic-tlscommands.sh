#!/bin/bash
# added 2018-04-27 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that imdtls applies OpenSSL TLS config commands. The sender is
# configured to fail the handshake, and the expected TLS diagnostic is the oracle.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
generate_conf
PORT_RCVR_FILE="$RSYSLOG_DYNNAME.imdtls.port"

add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module(	load="../plugins/imdtls/.libs/imdtls" )
input(	type="imdtls"
	tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.1,-TLSv1.3
	Options=Bugs"
	port="0"
	listenPortFileName="'$PORT_RCVR_FILE'")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

# now inject the messages which will fail due protocol configuration
tcpflood --check-only -k "Protocol=-ALL,TLSv1.3" -p$PORT_RCVR -m$NUMMESSAGES -Tdtls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem

shutdown_when_empty
wait_shutdown

if content_check --check-only "TLS library does not support SSL_CONF_cmd"
then
	echo "SKIP: TLS library does not support SSL_CONF_cmd"
	skip_test
else
	if content_check --check-only "DTLSv1_listen"
	then
		# Found DTLSv1_listen error, no further check needed
		exit_test
	else
		# Check for OpenSSL Error Stack
		content_check "OpenSSL Error Stack:"
	fi
fi

exit_test
