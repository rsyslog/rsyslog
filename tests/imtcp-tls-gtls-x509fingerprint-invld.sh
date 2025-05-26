#!/bin/bash
# added 2018-12-22 by Rainer Gerhards
# check for INVALID fingerprint!
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
#	debug.whitelist="on"
#	debug.files=["net_ossl.c", "nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/fingerprint"
	PermittedPeer=["SHA1:FF:C6:62:D5:9D:25:9F:BC:F3:CB:61:FA:D2:B3:8B:61:88:D7:06:C3"] # INVALID!
	)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -p'$TCPFLOOD_PORT' -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem 
shutdown_when_empty
wait_shutdown
content_check --regex "peer fingerprint .* unknown"
exit_test
