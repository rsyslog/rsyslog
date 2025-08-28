#!/bin/bash
# Test for issue #5207: multiple intermediate CA certificates
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="log"

# Prepare certificate workspace (dynamic, auto-cleaned by testbench)
CERTDIR="$RSYSLOG_DYNNAME.certwork"
mkdir -p "$CERTDIR"

# Create minimal OpenSSL extfiles
cat >"$CERTDIR/ca_ext.cnf" <<'EOF'
[v3_ca]
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer
basicConstraints = critical, CA:true
keyUsage = critical, cRLSign, keyCertSign
EOF

cat >"$CERTDIR/leaf_ext.cnf" <<'EOF'
[server_cert]
basicConstraints = CA:false
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth

[client_cert]
basicConstraints = CA:false
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
EOF

# Generate root CA
openssl genpkey -algorithm RSA -out "$CERTDIR/ca-root-key.pem" >/dev/null 2>&1
openssl req -new -key "$CERTDIR/ca-root-key.pem" -out "$CERTDIR/ca-root.csr" -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Root-CA" >/dev/null 2>&1
openssl x509 -req -in "$CERTDIR/ca-root.csr" -signkey "$CERTDIR/ca-root-key.pem" -days 3650 \
  -extfile "$CERTDIR/ca_ext.cnf" -extensions v3_ca -out "$CERTDIR/ca-root-cert.pem" >/dev/null 2>&1

# Create intermediate CA 1
openssl genpkey -algorithm RSA -out "$CERTDIR/intermediate-ca1-key.pem" >/dev/null 2>&1
openssl req -new -key "$CERTDIR/intermediate-ca1-key.pem" -out "$CERTDIR/intermediate-ca1.csr" -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Intermediate-CA1" >/dev/null 2>&1
openssl x509 -req -in "$CERTDIR/intermediate-ca1.csr" -CA "$CERTDIR/ca-root-cert.pem" -CAkey "$CERTDIR/ca-root-key.pem" -CAcreateserial -days 1825 \
  -extfile "$CERTDIR/ca_ext.cnf" -extensions v3_ca -out "$CERTDIR/intermediate-ca1-cert.pem" >/dev/null 2>&1

# Create intermediate CA 2
openssl genpkey -algorithm RSA -out "$CERTDIR/intermediate-ca2-key.pem" >/dev/null 2>&1
openssl req -new -key "$CERTDIR/intermediate-ca2-key.pem" -out "$CERTDIR/intermediate-ca2.csr" -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=Intermediate-CA2" >/dev/null 2>&1
openssl x509 -req -in "$CERTDIR/intermediate-ca2.csr" -CA "$CERTDIR/ca-root-cert.pem" -CAkey "$CERTDIR/ca-root-key.pem" -CAcreateserial -days 1825 \
  -extfile "$CERTDIR/ca_ext.cnf" -extensions v3_ca -out "$CERTDIR/intermediate-ca2-cert.pem" >/dev/null 2>&1

# Create server certificate signed by intermediate CA 1
openssl genpkey -algorithm RSA -out "$CERTDIR/server-int1-key.pem" >/dev/null 2>&1
openssl req -new -key "$CERTDIR/server-int1-key.pem" -out "$CERTDIR/server-int1.csr" -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=rsyslogserver" >/dev/null 2>&1
openssl x509 -req -in "$CERTDIR/server-int1.csr" -CA "$CERTDIR/intermediate-ca1-cert.pem" -CAkey "$CERTDIR/intermediate-ca1-key.pem" -CAcreateserial -days 365 \
  -extfile "$CERTDIR/leaf_ext.cnf" -extensions server_cert -out "$CERTDIR/server-int1-cert.pem" >/dev/null 2>&1
cat "$CERTDIR/server-int1-cert.pem" "$CERTDIR/intermediate-ca1-cert.pem" > "$CERTDIR/server-int1-chain.pem"

# Create client certificate signed by intermediate CA 2
openssl genpkey -algorithm RSA -out "$CERTDIR/client-int2-key.pem" >/dev/null 2>&1
openssl req -new -key "$CERTDIR/client-int2-key.pem" -out "$CERTDIR/client-int2.csr" -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=rsyslogclient" >/dev/null 2>&1
openssl x509 -req -in "$CERTDIR/client-int2.csr" -CA "$CERTDIR/intermediate-ca2-cert.pem" -CAkey "$CERTDIR/intermediate-ca2-key.pem" -CAcreateserial -days 365 \
  -extfile "$CERTDIR/leaf_ext.cnf" -extensions client_cert -out "$CERTDIR/client-int2-cert.pem" >/dev/null 2>&1
cat "$CERTDIR/client-int2-cert.pem" "$CERTDIR/intermediate-ca2-cert.pem" > "$CERTDIR/client-int2-chain.pem"

# Debug: dump generated certificates and verify chains
{
  echo "==== CERT DEBUG: Subjects/Issuers/Dates/Fingerprints ===="
  echo "-- Root CA --";
  openssl x509 -in "$CERTDIR/ca-root-cert.pem" -noout -subject -issuer -dates -serial -fingerprint -sha256;
  echo "-- Intermediate CA 1 --";
  openssl x509 -in "$CERTDIR/intermediate-ca1-cert.pem" -noout -subject -issuer -dates -serial -fingerprint -sha256;
  echo "-- Intermediate CA 2 --";
  openssl x509 -in "$CERTDIR/intermediate-ca2-cert.pem" -noout -subject -issuer -dates -serial -fingerprint -sha256;
  echo "-- Server leaf --";
  openssl x509 -in "$CERTDIR/server-int1-cert.pem" -noout -subject -issuer -dates -serial -fingerprint -sha256;
  echo "-- Server chain (top of file) --";
  openssl x509 -in "$CERTDIR/server-int1-chain.pem" -noout -subject -issuer -dates -serial -fingerprint -sha256;
  echo "-- Client leaf --";
  openssl x509 -in "$CERTDIR/client-int2-cert.pem" -noout -subject -issuer -dates -serial -fingerprint -sha256;
  echo "-- Client chain (top of file) --";
  openssl x509 -in "$CERTDIR/client-int2-chain.pem" -noout -subject -issuer -dates -serial -fingerprint -sha256;
  echo "==== OPENSSL VERIFY ===="
  echo "Verify server leaf against Root + Intermediate1";
  openssl verify -CAfile "$CERTDIR/ca-root-cert.pem" -untrusted "$CERTDIR/intermediate-ca1-cert.pem" "$CERTDIR/server-int1-cert.pem";
  echo "Verify client leaf against Root + Intermediate2";
  openssl verify -CAfile "$CERTDIR/ca-root-cert.pem" -untrusted "$CERTDIR/intermediate-ca2-cert.pem" "$CERTDIR/client-int2-cert.pem";
}

generate_conf
export PORT_RCVR="$(get_free_port)"
export SERVER_CN="rsyslogserver"
export CLIENT_CN="rsyslogclient"

add_conf '
global(
    DefaultNetstreamDriver="ossl"
    DefaultNetstreamDriverCAFile="'$CERTDIR'/ca-root-cert.pem"
    DefaultNetstreamDriverCertFile="'$CERTDIR'/server-int1-chain.pem"
    DefaultNetstreamDriverKeyFile="'$CERTDIR'/server-int1-key.pem"
    NetstreamDriverCAExtraFiles="'$CERTDIR'/intermediate-ca1-cert.pem,'$CERTDIR'/intermediate-ca2-cert.pem"
)

module(  load="../plugins/imtcp/.libs/imtcp"
        StreamDriver.Name="ossl"
        StreamDriver.Mode="1"
        PermittedPeer="'$CLIENT_CN'"
        StreamDriver.AuthMode="x509/name" )
input(  type="imtcp" port="'$PORT_RCVR'" )

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'"
:msg, contains, "msgnum:" ?dynfile;outfmt
'

startup
export RSYSLOG_DEBUGLOG="log2"

generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)"
add_conf '
global(
    defaultNetstreamDriverCAFile="'$CERTDIR'/ca-root-cert.pem"
    defaultNetstreamDriverCertFile="'$CERTDIR'/client-int2-chain.pem"
    defaultNetstreamDriverKeyFile="'$CERTDIR'/client-int2-key.pem"
)

$ModLoad ../plugins/imtcp/.libs/imtcp
input(  type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

action( type="omfwd"
    protocol="tcp"
    target="127.0.0.1"
    port="'$PORT_RCVR'"
    StreamDriver="ossl"
    StreamDriverMode="1"
    StreamDriverAuthMode="x509/name"
    StreamDriverPermittedPeers="'$SERVER_CN'"
)
' 2

startup 2

tcpflood -m$NUMMESSAGES -i1
wait_file_lines

shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

seq_check 1 $NUMMESSAGES
exit_test

