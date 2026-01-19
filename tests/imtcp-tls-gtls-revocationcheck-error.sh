#!/bin/bash
# Test for StreamDriver.TlsRevocationCheck error with gtls driver
# Verifies that enabling OCSP revocation checking with GnuTLS driver
# produces an appropriate error message since GnuTLS does not support OCSP.
# added 2026-01-19 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)
module(	load="../plugins/imtcp/.libs/imtcp" )
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
	StreamDriver.TlsRevocationCheck="on" )

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
# Note: We do not send messages because the listener will fail to start
# due to the unsupported TlsRevocationCheck parameter.
startup
shutdown_when_empty
wait_shutdown
content_check "TLS revocation checking not supported by"
content_check "gtls netstream driver"
exit_test
