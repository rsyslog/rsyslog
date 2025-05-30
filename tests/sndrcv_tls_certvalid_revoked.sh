#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
printf 'using TLS driver: %s\n' ${RS_TLS_DRIVER:=gtls}

# start up the instances
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultnetstreamdriverCRLfile="'$srcdir/testsuites/x.509/crl.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert-new.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="'$RS_TLS_DRIVER'"
#	debug.whitelist="on"
#	debug.files=["net_ossl.c", "nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="'$RS_TLS_DRIVER'"
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
	defaultnetstreamdriverCRLfile="'$srcdir/testsuites/x.509/crl.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-revoked.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-revoked-key.pem'"
	defaultNetstreamDriver="'$RS_TLS_DRIVER'"
)

# set up the action
$ActionSendStreamDriverMode 1 # require TLS for the connection
$ActionSendStreamDriverAuthMode anon
*.*	@@127.0.0.1:'$PORT_RCVR'
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2

# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

if [ -z "$TEXT_TO_CHECK" ]; then
	TEXT_TO_CHECK="not permitted to talk to peer '.*', certificate invalid: certificate revoked"
fi
content_check --regex "$TEXT_TO_CHECK"

exit_test
