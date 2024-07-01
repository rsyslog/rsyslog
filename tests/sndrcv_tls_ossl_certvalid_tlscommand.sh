#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
# export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
# start up the instances
# export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="ossl"
#	debug.whitelist="on"
#	debug.files=["net_ossl.c", "nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/certvalid"
	StreamDriver.PermitExpiredCerts="off"
	gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2\nOptions=Bugs"
	)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
export PORT_RCVR=$TCPFLOOD_PORT # save this, will be rewritten with next config
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="ossl"
)

action(	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	StreamDriverMode="1"
	StreamDriverAuthMode="x509/certvalid"
	gnutlsPriorityString="Protocol=-ALL,TLSv1.2"
)
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2 0 1
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

# IMPORTANT: this test will generate many error messages. This is exactly it's
# intent. So do not think something is wrong. The content_check below checks
# these error codes.

content_check --check-only "TLS library does not support SSL_CONF_cmd"
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: TLS library does not support SSL_CONF_cmd"
	skip_test
else
	content_check --check-only "SSL_ERROR_SYSCALL"
	ret=$?
	if [ $ret == 0 ]; then
		# Found SSL_ERROR_SYSCALL errorcode, no further check needed
		exit_test
	else
		# Check for a SSL_ERROR_SSL error code
		content_check "SSL_ERROR_SSL"
		content_check "OpenSSL Error Stack:"
	fi
fi

exit_test
