#!/bin/bash
# Verify strict OpenSSL x509/name behavior when StreamDriver.PrioritizeSAN is
# enabled. The fixture certificate has CN=rsyslog-client and DNS SAN
# testbench.rsyslog.com. Authorizing only the CN must fail when any SAN exists;
# otherwise a certificate with an unrelated SAN could bypass SAN-first identity
# policy. The oracle is the receiver-side "peer name not authorized" diagnostic
# after a checked tcpflood attempt and synchronized shutdown.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=3
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/name"
	StreamDriver.PrioritizeSAN="on"
	PermittedPeer="rsyslog-client"
	)
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood --check-only -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
content_check "peer name not authorized"
check_not_present "msgnum:"
exit_test
