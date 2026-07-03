#!/bin/bash
# Verify that strict pkcs11: builtin:omfwd module defaults are not inherited by
# a gtls omfwd action. The action relies on valid global file-backed TLS
# defaults, while the module deliberately supplies incompatible pkcs11: URIs.
# The oracle is that mutual TLS still succeeds and the OpenSSL helper captures
# exactly one forwarded payload line.
. ${srcdir:=.}/diag.sh init

check_command_available openssl

if [ ! -x ./openssl_mtls_server ]; then
	echo "openssl_mtls_server helper not built - skipping test"
	exit 77
fi

gtls_module="$(cd .. && pwd)/runtime/.libs/lmnsd_gtls.so"
if [ ! -f "$gtls_module" ]; then
	echo "lmnsd_gtls module not built - skipping test"
	exit 77
fi

workdir="$RSYSLOG_DYNNAME.omfwd-tls-gtls-module-pkcs11-defaults-isolation"
port_file="$workdir/server.port"
server_capture="$workdir/server.capture"
server_stderr="$workdir/server.stderr"
mkdir -p "$workdir"

cat >"$workdir/server.ext" <<'EOF'
basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost,IP:127.0.0.1
EOF

cat >"$workdir/client.ext" <<'EOF'
basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=clientAuth
EOF

cat >"$workdir/ca.cnf" <<'EOF'
[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_ca
prompt = no

[req_distinguished_name]

[v3_ca]
basicConstraints = critical,CA:TRUE,pathlen:0
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
EOF

openssl genrsa -out "$workdir/ca.key" 2048 || error_exit 1
openssl req -new -key "$workdir/ca.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=Test-CA" \
	-out "$workdir/ca.csr" || error_exit 1
openssl x509 -req -in "$workdir/ca.csr" -signkey "$workdir/ca.key" -sha256 -days 365 \
	-extfile "$workdir/ca.cnf" -extensions v3_ca \
	-out "$workdir/ca.pem" || error_exit 1

openssl genrsa -out "$workdir/server.key" 2048 || error_exit 1
openssl req -new -key "$workdir/server.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=localhost" \
	-out "$workdir/server.csr" || error_exit 1
openssl x509 -req -in "$workdir/server.csr" -CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/server.ext" \
	-out "$workdir/server.pem" || error_exit 1

openssl genrsa -out "$workdir/client.key" 2048 || error_exit 1
openssl req -new -key "$workdir/client.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=rsyslog-client" \
	-out "$workdir/client.csr" || error_exit 1
openssl x509 -req -in "$workdir/client.csr" -CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/client.ext" \
	-out "$workdir/client.pem" || error_exit 1

./openssl_mtls_server 0 "$workdir/server.pem" "$workdir/server.key" "$workdir/ca.pem" \
	"$port_file" "$server_capture" 2>"$server_stderr" &
server_pid=$!
wait_file_exists_for_process "$port_file" "$server_pid" 10 "OpenSSL MTLS helper" "$server_stderr"
server_port=$(cat "$port_file")

generate_conf
add_conf '
global(
	defaultNetstreamDriverCAFile="'$workdir/ca.pem'"
	defaultNetstreamDriverCertFile="'$workdir/client.pem'"
	defaultNetstreamDriverKeyFile="'$workdir/client.key'"
	compatibility.defaults.secure="strict"
	net.ipprotocol="ipv4-only"
)

module(load="'$gtls_module'")
module(
	load="builtin:omfwd"
	StreamDriver.CAFile="pkcs11:token=rsyslog-test-token;object=ca-cert;type=cert"
	StreamDriver.CertFile="pkcs11:token=rsyslog-test-token;object=client-cert;type=cert"
	StreamDriver.KeyFile="pkcs11:token=rsyslog-test-token;object=client-key;type=private"
)
template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "omfwd-gtls-module-pkcs11-defaults-isolation" then {
	action(
		type="omfwd"
		target="127.0.0.1"
		protocol="tcp"
		port="'$server_port'"
		template="outfmt"
		StreamDriver="gtls"
		StreamDriverMode="1"
		StreamDriverAuthMode="x509/certvalid"
	)
}
'

startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - omfwd-gtls-module-pkcs11-defaults-isolation'
wait_file_lines "$server_capture" 1
shutdown_when_empty
wait_shutdown

wait $server_pid || {
	cat "$server_stderr"
	error_exit 1
}

content_count_check --regex '^omfwd-gtls-module-pkcs11-defaults-isolation$' 1 "$server_capture"

exit_test
