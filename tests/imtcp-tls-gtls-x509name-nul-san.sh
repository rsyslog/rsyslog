#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0.
# Defense-in-depth regression test for GnuTLS x509/name matching.
#
# The generated client certificate is signed by the trusted test CA but contains
# SAN dNSName bytes "rsyslog-client\0.attacker" while its CN is deliberately
# non-matching. A compliant CA should not issue such a malformed DNS identity,
# and a compromised CA could simply issue the exact permitted name, so this is a
# hardening invariant rather than a practical CA-bypass model. The oracle is
# rejection: rsyslog must report an unauthorized peer name and must not accept
# the test message through C-string prefix interpretation of the raw SAN bytes.
. ${srcdir:=.}/diag.sh init
check_command_available openssl
check_command_available python3
export NUMMESSAGES=1
CERTDIR="$RSYSLOG_DYNNAME.nul-san-certs"
mkdir -p "$CERTDIR"

openssl genrsa -out "$CERTDIR/client-key.pem" 2048 >/dev/null 2>&1
openssl req -new -key "$CERTDIR/client-key.pem" -out "$CERTDIR/client.csr" \
	-subj "/C=US/ST=Test/O=rsyslog/CN=malicious-client" >/dev/null 2>&1
cat > "$CERTDIR/client-ext.cnf" <<'EOF'
[usr_cert]
basicConstraints=CA:FALSE
# subjectAltName = SEQUENCE { dNSName "rsyslog-client\0.attacker" }
subjectAltName=DER:30:1a:82:18:72:73:79:73:6c:6f:67:2d:63:6c:69:65:6e:74:00:2e:61:74:74:61:63:6b:65:72
EOF
openssl x509 -req -in "$CERTDIR/client.csr" \
	-CA "$srcdir/tls-certs/ca.pem" -CAkey "$srcdir/tls-certs/ca-key.pem" -set_serial 01 \
	-out "$CERTDIR/client-cert.pem" -days 365 -sha256 \
	-extfile "$CERTDIR/client-ext.cnf" -extensions usr_cert >/dev/null 2>&1

# Sanity-check that the fixture really contains the embedded-NUL SAN bytes.
openssl x509 -in "$CERTDIR/client-cert.pem" -outform DER -out "$CERTDIR/client-cert.der"
python3 - "$CERTDIR/client-cert.der" <<'PY'
from pathlib import Path
import sys
cert = Path(sys.argv[1]).read_bytes()
if b"rsyslog-client\x00.attacker" not in cert:
    raise SystemExit("embedded-NUL SAN fixture was not generated")
PY

generate_conf
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
        defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
        defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module( load="../plugins/imtcp/.libs/imtcp"
        StreamDriver.Name="gtls"
        StreamDriver.Mode="1"
        StreamDriver.AuthMode="x509/name"
        PermittedPeer=["rsyslog-client"]
)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -p"$TCPFLOOD_PORT" -m"$NUMMESSAGES" -Ttls \
	-x"$srcdir/tls-certs/ca.pem" \
	-Z"$CERTDIR/client-cert.pem" \
	-z"$CERTDIR/client-key.pem" || true
shutdown_when_empty
wait_shutdown
content_check --regex "peer name not authorized"
check_not_present "msgnum:00000000" "$RSYSLOG_OUT_LOG"
exit_test
