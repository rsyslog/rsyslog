#!/bin/bash
# Verify that builtin:omfwd module defaults can supply PKCS#11-backed TLS
# objects for an omfwd action that omits action-level StreamDriver.CAFile,
# StreamDriver.CAExtraFiles, StreamDriver.CertFile, and StreamDriver.KeyFile.
# The server certificate is signed by an intermediate CA that is intentionally
# not sent by the helper server, so the client handshake succeeds only if
# omfwd loads the root CA via StreamDriver.CAFile and the missing intermediate
# via StreamDriver.CAExtraFiles. The oracle is that the OpenSSL helper, which
# also requires a valid client certificate, captures exactly one forwarded
# payload line. All PKCS#11 state is created inside a per-test directory so
# parallel runs do not collide.
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

workdir="$RSYSLOG_DYNNAME.omfwd-tls-ossl-pkcs11-module-mtls"
port_file="$workdir/server.port"
server_capture="$workdir/server.capture"
server_stderr="$workdir/server.stderr"
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
basicConstraints=critical,CA:TRUE,pathlen:1
keyUsage=critical,keyCertSign,cRLSign
subjectKeyIdentifier=hash
EOF

cat >"$workdir/intermediate.ext" <<'EOF'
basicConstraints=critical,CA:TRUE,pathlen:0
keyUsage=critical,keyCertSign,cRLSign
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
EOF

openssl genrsa -out "$workdir/ca.key" 2048 || error_exit 1
openssl req -new -key "$workdir/ca.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=Test-CA" \
	-out "$workdir/ca.csr" || error_exit 1
openssl x509 -req -in "$workdir/ca.csr" -signkey "$workdir/ca.key" -sha256 -days 365 \
	-extfile "$workdir/ca.cnf" -extensions v3_ca \
	-out "$workdir/ca.pem" || error_exit 1

openssl genrsa -out "$workdir/intermediate.key" 2048 || error_exit 1
openssl req -new -key "$workdir/intermediate.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=Intermediate-CA" \
	-out "$workdir/intermediate.csr" || error_exit 1
openssl x509 -req -in "$workdir/intermediate.csr" -CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/intermediate.ext" \
	-out "$workdir/intermediate.pem" || error_exit 1

openssl genrsa -out "$workdir/server.key" 2048 || error_exit 1
openssl req -new -key "$workdir/server.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=localhost" \
	-out "$workdir/server.csr" || error_exit 1
openssl x509 -req -in "$workdir/server.csr" -CA "$workdir/intermediate.pem" -CAkey "$workdir/intermediate.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/server.ext" \
	-out "$workdir/server.pem" || error_exit 1

openssl genrsa -out "$workdir/client.key" 2048 || error_exit 1
openssl req -new -key "$workdir/client.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=rsyslog-client" \
	-out "$workdir/client.csr" || error_exit 1
openssl x509 -req -in "$workdir/client.csr" -CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/client.ext" \
	-out "$workdir/client.pem" || error_exit 1

openssl x509 -in "$workdir/ca.pem" -outform DER -out "$workdir/ca.der" || error_exit 1
openssl x509 -in "$workdir/intermediate.pem" -outform DER -out "$workdir/intermediate.der" || error_exit 1
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
	--write-object "$workdir/ca.der" --type cert --id 02 --label ca-cert || error_exit 1
pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/intermediate.der" --type cert --id 03 --label intermediate-ca-cert || error_exit 1

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
extra_ca_uri="pkcs11:token=$token_label;object=intermediate-ca-cert;type=cert"
cert_uri="pkcs11:token=$token_label;object=client-cert;type=cert"
key_uri="pkcs11:token=$token_label;object=client-key;type=private"

./openssl_mtls_server 0 "$workdir/server.pem" "$workdir/server.key" "$workdir/ca.pem" \
	"$port_file" "$server_capture" 2>"$server_stderr" &
server_pid=$!
wait_file_exists_for_process "$port_file" "$server_pid" 10 "OpenSSL MTLS helper" "$server_stderr"
server_port=$(cat "$port_file")

generate_conf
add_conf '
global(
	compatibility.defaults.secure="strict"
	net.ipprotocol="ipv4-only"
)

module(
	load="builtin:omfwd"
	StreamDriver.CAFile="'$ca_uri'"
	StreamDriver.CAExtraFiles="'$extra_ca_uri'"
	StreamDriver.CertFile="'$cert_uri'"
	StreamDriver.KeyFile="'$key_uri'"
)
template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "omfwd-ossl-pkcs11-module-mtls" then {
	action(
		type="omfwd"
		target="127.0.0.1"
		protocol="tcp"
		port="'$server_port'"
		template="outfmt"
		StreamDriver="ossl"
		StreamDriverMode="1"
		StreamDriverAuthMode="x509/certvalid"
	)
}
'

startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - omfwd-ossl-pkcs11-module-mtls'
wait_file_lines "$server_capture" 1
shutdown_when_empty
wait_shutdown

wait $server_pid || {
	cat "$server_stderr"
	error_exit 1
}

content_count_check --regex '^omfwd-ossl-pkcs11-module-mtls$' 1 "$server_capture"

exit_test
