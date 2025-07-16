#!/bin/bash
# Test for issue #5207: multiple intermediate CA certificates
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
generate_conf
export PORT_RCVR="$(get_free_port)"
### This is important, as it must be exactly the same
### as the ones configured in used certificates
export HOSTNAME="fedora"

# Create intermediate CA certificates and client/server certificates
# This simulates the scenario where:
# - Root CA issues multiple intermediate CAs
# - Each intermediate CA issues client/server certificates
# - Client presents intermediate CA to server
# - Server only knows about root CA

# Generate root CA (if not already present)
openssl genpkey -algorithm RSA -out testsuites/certchain/ca-root-key.pem
openssl req -x509 -new -key testsuites/certchain/ca-root-key.pem -out testsuites/certchain/ca-root-cert.pem -days 3650 -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Root-CA"

# Create intermediate CA 1
openssl genpkey -algorithm RSA -out testsuites/certchain/intermediate-ca1-key.pem
openssl req -new -key testsuites/certchain/intermediate-ca1-key.pem -out testsuites/certchain/intermediate-ca1.csr -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Intermediate-CA1"
openssl x509 -req -in testsuites/certchain/intermediate-ca1.csr -CA testsuites/certchain/ca-root-cert.pem -CAkey testsuites/certchain/ca-root-key.pem -CAcreateserial -out testsuites/certchain/intermediate-ca1-cert.pem -days 365

# Create intermediate CA 2
openssl genpkey -algorithm RSA -out testsuites/certchain/intermediate-ca2-key.pem
openssl req -new -key testsuites/certchain/intermediate-ca2-key.pem -out testsuites/certchain/intermediate-ca2.csr -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Intermediate-CA2"
openssl x509 -req -in testsuites/certchain/intermediate-ca2.csr -CA testsuites/certchain/ca-root-cert.pem -CAkey testsuites/certchain/ca-root-key.pem -CAcreateserial -out testsuites/certchain/intermediate-ca2-cert.pem -days 365

# Create server certificate signed by intermediate CA 1
openssl genpkey -algorithm RSA -out testsuites/certchain/server-int1-key.pem
openssl req -new -key testsuites/certchain/server-int1-key.pem -out testsuites/certchain/server-int1.csr -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=fedora"
openssl x509 -req -in testsuites/certchain/server-int1.csr -CA testsuites/certchain/intermediate-ca1-cert.pem -CAkey testsuites/certchain/intermediate-ca1-key.pem -CAcreateserial -out testsuites/certchain/server-int1-cert.pem -days 365

# Create client certificate signed by intermediate CA 2
openssl genpkey -algorithm RSA -out testsuites/certchain/client-int2-key.pem
openssl req -new -key testsuites/certchain/client-int2-key.pem -out testsuites/certchain/client-int2.csr -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=client"
openssl x509 -req -in testsuites/certchain/client-int2.csr -CA testsuites/certchain/intermediate-ca2-cert.pem -CAkey testsuites/certchain/intermediate-ca2-key.pem -CAcreateserial -out testsuites/certchain/client-int2-cert.pem -days 365

# Create certificate chains
cat testsuites/certchain/server-int1-cert.pem testsuites/certchain/intermediate-ca1-cert.pem > testsuites/certchain/server-int1-chain.pem
cat testsuites/certchain/client-int2-cert.pem testsuites/certchain/intermediate-ca2-cert.pem > testsuites/certchain/client-int2-chain.pem

add_conf '
global(
    DefaultNetstreamDriver="ossl"
    DefaultNetstreamDriverCAFile="'$srcdir/testsuites/certchain/ca-root-cert.pem'"
    DefaultNetstreamDriverCertFile="'$srcdir/testsuites/certchain/server-int1-chain.pem'"
    DefaultNetstreamDriverKeyFile="'$srcdir/testsuites/certchain/server-int1-key.pem'"
    NetstreamDriverCAExtraFiles="'$srcdir/testsuites/certchain/intermediate-ca1-cert.pem'"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
        PermittedPeer="'$HOSTNAME'"
	StreamDriver.AuthMode="x509/name" )
# then SENDER sends to this port (not tcpflood!)
input(	type="imtcp" port="'$PORT_RCVR'" )

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)"
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/certchain/ca-root-cert.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/certchain/client-int2-chain.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/certchain/client-int2-key.pem'"
)

# Note: no TLS for the listener, this is for tcpflood!
$ModLoad ../plugins/imtcp/.libs/imtcp
input(	type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

# set up the action
action(	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	StreamDriver="ossl"
	StreamDriverMode="1"
	StreamDriverAuthMode="x509/name"
        StreamDriverPermittedPeers="'$HOSTNAME'"
	)
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m$NUMMESSAGES -i1
wait_file_lines
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check 1 $NUMMESSAGES
exit_test