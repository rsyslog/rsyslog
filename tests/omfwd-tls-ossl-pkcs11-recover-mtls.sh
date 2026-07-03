#!/bin/bash
# Verify that omfwd with the ossl driver recovers from a TLS handshake failure
# when the client-side CA certificate, client certificate, and client private
# key are all loaded from valid pkcs11: URIs. The test first presents a server
# certificate signed by an untrusted CA so the initial handshake fails even
# though all client-side PKCS#11 objects resolve successfully. It then restarts
# a server on the same port with a certificate signed by the trusted CA and
# verifies that omfwd retries automatically and eventually delivers the queued
# message.
. ${srcdir:=.}/diag.sh init

check_command_available openssl
check_command_available softhsm2-util
check_command_available pkcs11-tool

if [ ! -x ./openssl_mtls_server ]; then
	echo "openssl_mtls_server helper not built - skipping test"
	exit 77
fi

find_first_existing() {
	for candidate in "$@"; do
		if [ -f "$candidate" ]; then
			printf '%s\n' "$candidate"
			return 0
		fi
	done
	return 1
}

wait_pid_exit_with_timeout() {
	pid="$1"
	while kill -0 "$pid" 2>/dev/null; do
		$TESTTOOL_DIR/msleep 100
		if [ "$(date +%s)" -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
			printf 'TESTBENCH error: timeout waiting for helper pid %s to exit\n' "$pid"
			ps -fp "$pid"
			error_exit 1
		fi
	done
	wait "$pid"
}

workdir="$RSYSLOG_DYNNAME.omfwd-tls-ossl-pkcs11-recover-mtls"
port_file="$workdir/server.port"
bad_capture="$workdir/server-bad.capture"
bad_stderr="$workdir/server-bad.stderr"
good_capture="$workdir/server-good.capture"
good_stderr="$workdir/server-good.stderr"
softhsm_conf="$workdir/softhsm2.conf"
openssl_conf="$workdir/openssl-pkcs11.cnf"
token_dir="$workdir/tokens"
token_label="rsyslog-test-token"
token_pin="123456"
token_so_pin="12345678"
mkdir -p "$workdir" "$token_dir"

provider_module=$(find_first_existing \
	/usr/lib64/ossl-modules/pkcs11.so \
	/usr/lib/ossl-modules/pkcs11.so \
	/usr/lib/x86_64-linux-gnu/ossl-modules/pkcs11.so \
	/usr/lib/aarch64-linux-gnu/ossl-modules/pkcs11.so \
	/usr/local/lib64/ossl-modules/pkcs11.so \
	/usr/local/lib/ossl-modules/pkcs11.so) || {
	printf 'info: skipping test - pkcs11 OpenSSL provider module not found\n'
	skip_test
}

softhsm_module=$(find_first_existing \
	/usr/lib64/pkcs11/libsofthsm2.so \
	/usr/lib/pkcs11/libsofthsm2.so \
	/usr/lib64/softhsm/libsofthsm2.so \
	/usr/lib/softhsm/libsofthsm2.so \
	/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so \
	/usr/lib/aarch64-linux-gnu/softhsm/libsofthsm2.so \
	/usr/lib64/libsofthsm2.so \
	/usr/lib/libsofthsm2.so) || {
	printf 'info: skipping test - SoftHSM PKCS#11 module not found\n'
	skip_test
}

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

openssl genrsa -out "$workdir/trusted-ca.key" 2048 || error_exit 1
openssl req -x509 -new -key "$workdir/trusted-ca.key" -sha256 -days 365 \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=Trusted-CA" \
	-config "$workdir/ca.cnf" -extensions v3_ca \
	-out "$workdir/trusted-ca.pem" || error_exit 1

openssl genrsa -out "$workdir/untrusted-ca.key" 2048 || error_exit 1
openssl req -x509 -new -key "$workdir/untrusted-ca.key" -sha256 -days 365 \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=Untrusted-CA" \
	-config "$workdir/ca.cnf" -extensions v3_ca \
	-out "$workdir/untrusted-ca.pem" || error_exit 1

openssl genrsa -out "$workdir/client.key" 2048 || error_exit 1
openssl req -new -key "$workdir/client.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=rsyslog-client" \
	-out "$workdir/client.csr" || error_exit 1
openssl x509 -req -in "$workdir/client.csr" -CA "$workdir/trusted-ca.pem" -CAkey "$workdir/trusted-ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/client.ext" \
	-out "$workdir/client.pem" || error_exit 1

openssl genrsa -out "$workdir/server-bad.key" 2048 || error_exit 1
openssl req -new -key "$workdir/server-bad.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=localhost" \
	-out "$workdir/server-bad.csr" || error_exit 1
openssl x509 -req -in "$workdir/server-bad.csr" -CA "$workdir/untrusted-ca.pem" -CAkey "$workdir/untrusted-ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/server.ext" \
	-out "$workdir/server-bad.pem" || error_exit 1

openssl genrsa -out "$workdir/server-good.key" 2048 || error_exit 1
openssl req -new -key "$workdir/server-good.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=localhost" \
	-out "$workdir/server-good.csr" || error_exit 1
openssl x509 -req -in "$workdir/server-good.csr" -CA "$workdir/trusted-ca.pem" -CAkey "$workdir/trusted-ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/server.ext" \
	-out "$workdir/server-good.pem" || error_exit 1

openssl x509 -in "$workdir/trusted-ca.pem" -outform DER -out "$workdir/trusted-ca.der" || \
	error_exit 1
openssl x509 -in "$workdir/client.pem" -outform DER -out "$workdir/client.der" || error_exit 1
openssl pkey -in "$workdir/client.key" -pubout -outform DER -out "$workdir/client.pub.der" || \
	error_exit 1

cat >"$softhsm_conf" <<EOF
directories.tokendir = $token_dir
objectstore.backend = file
slots.removable = false
EOF
export SOFTHSM2_CONF="$softhsm_conf"

softhsm2-util --init-token --free --label "$token_label" --so-pin "$token_so_pin" --pin "$token_pin" || error_exit 1
softhsm2-util --import "$workdir/client.key" --token "$token_label" --label client-key --id 01 --pin "$token_pin" || error_exit 1

pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/client.pub.der" --type pubkey --id 01 --label client-pub || error_exit 1
pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/client.der" --type cert --id 01 --label client-cert || error_exit 1
pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/trusted-ca.der" --type cert --id 02 --label ca-cert || error_exit 1

cat >"$openssl_conf" <<EOF
openssl_conf = openssl_init

[openssl_init]
providers = provider_sect

[provider_sect]
default = default_sect
pkcs11 = pkcs11_sect

[default_sect]
activate = 1

[pkcs11_sect]
identity = pkcs11
module = $provider_module
pkcs11-module-path = $softhsm_module
pkcs11-module-token-pin = $token_pin
activate = 1
EOF
export OPENSSL_CONF="$openssl_conf"

ca_uri="pkcs11:token=$token_label;object=ca-cert;type=cert"
cert_uri="pkcs11:token=$token_label;object=client-cert;type=cert"
key_uri="pkcs11:token=$token_label;object=client-key;type=private"

./openssl_mtls_server 0 "$workdir/server-bad.pem" "$workdir/server-bad.key" "$workdir/trusted-ca.pem" \
	"$port_file" "$bad_capture" 2>"$bad_stderr" &
bad_server_pid=$!
wait_file_exists_for_process "$port_file" "$bad_server_pid" 10 "OpenSSL MTLS helper" "$bad_stderr"
server_port=$(cat "$port_file")

generate_conf
add_conf '
global(
	compatibility.defaults.secure="strict"
	net.ipprotocol="ipv4-only"
)

module(load="builtin:omfwd")
template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "omfwd-ossl-pkcs11-recover-mtls" then {
	action(
		type="omfwd"
		target="127.0.0.1"
		protocol="tcp"
		port="'$server_port'"
		template="outfmt"
		StreamDriver="ossl"
		StreamDriverMode="1"
		StreamDriverAuthMode="x509/certvalid"
		streamdriver.cafile="'$ca_uri'"
		streamdriver.certfile="'$cert_uri'"
		streamdriver.keyfile="'$key_uri'"
		pool.resumeInterval="1"
		action.resumeRetryCount="-1"
		action.resumeInterval="1"
	)
}
'

startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - omfwd-ossl-pkcs11-recover-mtls'

wait_content "SSL_accept failed" "$bad_stderr"
if wait_pid_exit_with_timeout $bad_server_pid; then
	printf 'Testbench ERROR: expected the first helper server to fail the TLS handshake\n'
	error_exit 1
fi

./openssl_mtls_server "$server_port" "$workdir/server-good.pem" "$workdir/server-good.key" "$workdir/trusted-ca.pem" \
	"$workdir/server-good.port" "$good_capture" 2>"$good_stderr" &
good_server_pid=$!
wait_file_exists_for_process "$workdir/server-good.port" "$good_server_pid" 10 "OpenSSL MTLS helper" "$good_stderr"

wait_file_lines "$good_capture" 1
shutdown_when_empty
wait_shutdown

wait_pid_exit_with_timeout $good_server_pid || {
	cat "$good_stderr"
	error_exit 1
}

content_count_check --regex '^omfwd-ossl-pkcs11-recover-mtls$' 1 "$good_capture"

exit_test
