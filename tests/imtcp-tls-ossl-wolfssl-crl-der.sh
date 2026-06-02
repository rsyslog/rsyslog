#!/bin/bash
# Test wolfSSL DER CRL handling.  The receiver is configured with the existing
# CRL converted to DER while the sender presents a revoked certificate; success
# is proved by receiver-side TLS rejection and the absence of delivered payload
# messages.  The sender deliberately has no CRL configured, so only the
# receiver-side DER CRL can cause the revoked client certificate to be rejected.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
check_command_available openssl

export NUMMESSAGES=1000
DER_CRL="$RSYSLOG_DYNNAME.crl.der"
openssl crl -in "$srcdir/testsuites/x.509/crl.pem" -outform DER -out "$DER_CRL" || \
	error_exit 1 "could not convert CRL to DER"

export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultnetstreamdriverCRLfile="'$DER_CRL'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert-new.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="ossl"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/certvalid"
	StreamDriver.PermitExpiredCerts="off"
	)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
export PORT_RCVR=$TCPFLOOD_PORT
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-revoked.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-revoked-key.pem'"
	defaultNetstreamDriver="ossl"
)

$ActionSendStreamDriverMode 1
$ActionSendStreamDriverAuthMode anon
*.*	@@127.0.0.1:'$PORT_RCVR'
' 2
startup 2

injectmsg2

shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

content_check "Handshake failed" "${RSYSLOG_DYNNAME}.started"
assert_content_missing "msgnum:"

exit_test
