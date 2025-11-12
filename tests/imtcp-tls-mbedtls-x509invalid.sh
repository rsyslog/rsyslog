#!/bin/bash
# added 2018-04-27 by alorbach
# This file is part of the rsyslog project, released  under GPLv3
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10000
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'" # wrong CA
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="mbedtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/certvalid" )
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
# Begin actual testcase
startup
tcpflood --check-only -p'$TCPFLOOD_PORT' -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
wait_shutdown
content_check "X509 - Certificate verification failed"
exit_test
