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

# Create certificate directory structure
CERT_DIR="${RSYSLOG_DYNNAME}.certchain"
mkdir -p $CERT_DIR

# Generate root CA
openssl genpkey -algorithm RSA -out $CERT_DIR/ca-root-key.pem 2>/dev/null
openssl req -x509 -new -key $CERT_DIR/ca-root-key.pem -out $CERT_DIR/ca-root-cert.pem -days 3650 \
    -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Root-CA" 2>/dev/null

# Create intermediate CA 1
openssl genpkey -algorithm RSA -out $CERT_DIR/intermediate-ca1-key.pem 2>/dev/null
openssl req -new -key $CERT_DIR/intermediate-ca1-key.pem -out $CERT_DIR/intermediate-ca1.csr \
    -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Intermediate-CA1" 2>/dev/null
openssl x509 -req -in $CERT_DIR/intermediate-ca1.csr -CA $CERT_DIR/ca-root-cert.pem \
    -CAkey $CERT_DIR/ca-root-key.pem -CAcreateserial -out $CERT_DIR/intermediate-ca1-cert.pem \
    -days 365 -extensions v3_ca 2>/dev/null

# Create intermediate CA 2
openssl genpkey -algorithm RSA -out $CERT_DIR/intermediate-ca2-key.pem 2>/dev/null
openssl req -new -key $CERT_DIR/intermediate-ca2-key.pem -out $CERT_DIR/intermediate-ca2.csr \
    -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Intermediate-CA2" 2>/dev/null
openssl x509 -req -in $CERT_DIR/intermediate-ca2.csr -CA $CERT_DIR/ca-root-cert.pem \
    -CAkey $CERT_DIR/ca-root-key.pem -CAcreateserial -out $CERT_DIR/intermediate-ca2-cert.pem \
    -days 365 -extensions v3_ca 2>/dev/null

# Create server certificate signed by intermediate CA 1
openssl genpkey -algorithm RSA -out $CERT_DIR/server-key.pem 2>/dev/null
openssl req -new -key $CERT_DIR/server-key.pem -out $CERT_DIR/server.csr \
    -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=$HOSTNAME" 2>/dev/null
openssl x509 -req -in $CERT_DIR/server.csr -CA $CERT_DIR/intermediate-ca1-cert.pem \
    -CAkey $CERT_DIR/intermediate-ca1-key.pem -CAcreateserial -out $CERT_DIR/server-cert.pem \
    -days 365 2>/dev/null

# Create client certificate signed by intermediate CA 2
openssl genpkey -algorithm RSA -out $CERT_DIR/client-key.pem 2>/dev/null
openssl req -new -key $CERT_DIR/client-key.pem -out $CERT_DIR/client.csr \
    -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=client" 2>/dev/null
openssl x509 -req -in $CERT_DIR/client.csr -CA $CERT_DIR/intermediate-ca2-cert.pem \
    -CAkey $CERT_DIR/intermediate-ca2-key.pem -CAcreateserial -out $CERT_DIR/client-cert.pem \
    -days 365 2>/dev/null

# Create certificate chains
cat $CERT_DIR/server-cert.pem $CERT_DIR/intermediate-ca1-cert.pem > $CERT_DIR/server-chain.pem
cat $CERT_DIR/client-cert.pem $CERT_DIR/intermediate-ca2-cert.pem > $CERT_DIR/client-chain.pem

add_conf '
global(
    DefaultNetstreamDriver="ossl"
    DefaultNetstreamDriverCAFile="'$CERT_DIR/ca-root-cert.pem'"
    DefaultNetstreamDriverCertFile="'$CERT_DIR/server-chain.pem'"
    DefaultNetstreamDriverKeyFile="'$CERT_DIR/server-key.pem'"
    NetstreamDriverCAExtraFiles="'$CERT_DIR/intermediate-ca1-cert.pem','$CERT_DIR/intermediate-ca2-cert.pem'"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
        PermittedPeer="client"
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
	defaultNetstreamDriverCAFile="'$CERT_DIR/ca-root-cert.pem'"
	defaultNetstreamDriverCertFile="'$CERT_DIR/client-chain.pem'"
	defaultNetstreamDriverKeyFile="'$CERT_DIR/client-key.pem'"
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

# Clean up certificate directory
rm -rf $CERT_DIR

exit_test