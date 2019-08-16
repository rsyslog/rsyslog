#!/bin/bash
# added 2018-12-22 by alorbach
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
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/name"
	PermittedPeer=["INVALID"]
	)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -p'$TCPFLOOD_PORT' -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
content_check --regex "peer name not authorized"
exit_test
