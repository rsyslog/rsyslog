#!/bin/bash
# added 2018-11-07 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export PORT_RCVR="$(get_free_port)"

add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert-fail.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/cert.pem'"
)

module(	load="../plugins/imdtls/.libs/imdtls" )
input(	type="imdtls"
	port="'$PORT_RCVR'")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
# note: we do not need to generate any messages, config error occurs on startup
startup

sleep 5 # TODO: FIXME - just checking if we terminate too early
shutdown_when_empty
wait_shutdown
content_check "Error: Certificate file could not be accessed"
content_check "OpenSSL Error Stack:"
exit_test
