#!/bin/bash
# added 2018-11-07 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that imdtls reports certificate load errors during startup. No traffic
# is sent; the configured rsyslog diagnostics are the pass/fail oracle.
. ${srcdir:=.}/diag.sh init
generate_conf

add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert-fail.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/cert.pem'"
)

module(	load="../plugins/imdtls/.libs/imdtls" )
input(	type="imdtls"
	port="0")

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
