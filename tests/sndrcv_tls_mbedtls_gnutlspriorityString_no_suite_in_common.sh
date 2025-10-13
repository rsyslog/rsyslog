#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module( load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="mbedtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/name"
	PermittedPeer="rsyslog client"
	gnutlspriorityString="TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384")
# then SENDER sends to this port (not tcpflood!)
input(	type="imtcp" port="'$PORT_RCVR'" )

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
#export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)"
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
)

# Note: no TLS for the listener, this is for tcpflood!
module( load="../plugins/imtcp/.libs/imtcp")
input( type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# set up the action
action( type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	StreamDriver="mbedtls"
	StreamDriverMode="1"
	StreamDriverAuthMode="x509/name"
	StreamDriverPermittedPeers="testbench.rsyslog.com"
	gnutlspriorityString="TLS-ECDHE-ECDSA-WITH-CAMELLIA-256-CBC-SHA384,TLS-ECDHE-ECDSA-WITH-CAMELLIA-256-GCM-SHA384")
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m$NUMMESSAGES -i1
# shut down sender when everything is sent, receiver continues to run concurrently
#shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
#shutdown_when_empty
wait_shutdown
content_check --regex "\(no ciphersuites in common\|The handshake negotiation failed\)"
exit_test
