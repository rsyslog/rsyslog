#!/bin/bash
# Verify the legacy OpenSSL x509/name behavior for certificates that contain a
# DNS SAN and a different CN. With PrioritizeSAN left at its default off value,
# rsyslog may authorize the peer by the CN after the SAN does not match. The
# oracle is exact receipt of the tcpflood sequence after the listener reports
# its kernel-assigned port.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
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
	PermittedPeer="rsyslog-client"
	)
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
