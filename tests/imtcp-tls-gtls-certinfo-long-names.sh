#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This is part of the rsyslog testbench, licensed under ASL 2.0.
# Coverage test for the GnuTLS peer certificate info dump (gtlsGetCertInfo).
#
# When a peer certificate fails validation, nsd_gtls logs the certificate's
# DN, issuer DN and SAN dNSNames. Names that do not fit the 1024-byte stack
# buffer are fetched again into a heap buffer that grows as needed. Regular
# test certificates have short names, so that growth path was never run.
#
# The client here presents an expired, CA-signed certificate whose
# subject DN is about 1.5 KB and whose single SAN dNSName is about 2 KB, so
# both the first move to the heap and a later growth of that heap buffer are
# exercised. The oracle: the connection is rejected as expired, the "invalid
# cert info" message carries the start of the long DN (fetched only after the
# buffer was moved to the heap), and rsyslogd shuts down cleanly. The -vg
# wrapper runs the same scenario under valgrind to catch leaks and invalid
# accesses on this path. LogError() caps a message at 2 KB, so the tail of
# the DN and the SAN entry cannot be checked in the log; they are covered by
# the clean (valgrind) run only.
. ${srcdir:=.}/diag.sh init
check_command_available openssl
export NUMMESSAGES=1
CERTDIR="$RSYSLOG_DYNNAME.long-names-certs"
mkdir -p "$CERTDIR"

# 24 OU RDNs of ~55 bytes each give a DN of ~1.5 KB; each RDN stays below
# OpenSSL's 64 character upper bound for OU.
OU_PAD=$(printf 'x%.0s' $(seq 50))
SUBJ="/C=US/O=rsyslog"
for i in $(seq -w 1 24); do
	SUBJ="$SUBJ/OU=ou$i-$OU_PAD"
done
SUBJ="$SUBJ/CN=longdn-client"
SAN_NAME="san-$(printf 'a%.0s' $(seq 2000)).longsan-end.example.net"

# Sign with the trusted test CA (a GnuTLS client sends its certificate only if
# the server lists its issuer) but make it expired, so validation fails.
# "openssl x509 -req" cannot set past dates on OpenSSL 3.0, so use a minimal
# throw-away "openssl ca" setup instead.
openssl req -new -newkey rsa:2048 -nodes \
	-keyout "$CERTDIR/client-key.pem" -out "$CERTDIR/client.csr" \
	-subj "$SUBJ" >/dev/null 2>&1
: > "$CERTDIR/index.txt"
echo 01 > "$CERTDIR/serial"
cat > "$CERTDIR/ca.cnf" <<CA_CNF
[ca]
default_ca = testca
[testca]
database = $CERTDIR/index.txt
new_certs_dir = $CERTDIR
serial = $CERTDIR/serial
default_md = sha256
policy = anything
unique_subject = no
[anything]
countryName = optional
organizationName = optional
organizationalUnitName = optional
commonName = optional
[usr_cert]
basicConstraints = CA:FALSE
subjectAltName = DNS:$SAN_NAME
CA_CNF
openssl ca -batch -notext -config "$CERTDIR/ca.cnf" -extensions usr_cert \
	-cert "$srcdir/tls-certs/ca.pem" -keyfile "$srcdir/tls-certs/ca-key.pem" \
	-startdate 20200101000000Z -enddate 20200102000000Z \
	-in "$CERTDIR/client.csr" -out "$CERTDIR/client-cert.pem" >/dev/null 2>&1
if [ ! -s "$CERTDIR/client-cert.pem" ]; then
	echo "FAIL: could not generate the long-name client certificate"
	error_exit 1
fi

generate_conf
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
        defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
        defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module( load="../plugins/imtcp/.libs/imtcp"
        StreamDriver.Name="gtls"
        StreamDriver.Mode="1"
        StreamDriver.AuthMode="x509/certvalid"
        StreamDriver.PermitExpiredCerts="off"
)
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -p"$TCPFLOOD_PORT" -m"$NUMMESSAGES" -Ttls \
	-x"$srcdir/tls-certs/ca.pem" \
	-Z"$CERTDIR/client-cert.pem" \
	-z"$CERTDIR/client-key.pem" || true
shutdown_when_empty
wait_shutdown
content_check --regex "not permitted to talk to peer '.*', certificate invalid: certificate expired"
content_check --regex "invalid cert info: .* DN: C=US,O=rsyslog,OU=ou01-xxxxx*,OU=ou02-xxxxx*,"
check_not_present "msgnum:00000000" "$RSYSLOG_OUT_LOG"
exit_test
