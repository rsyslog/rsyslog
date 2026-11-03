#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Shared body for the omfwd gtls PKCS#11 tests. GTLS_PKCS11_CASE selects:
#   mtls       CA, certificate and PIN-protected key all come from a SoftHSM
#              token through action-level pkcs11: URIs; the oracle is that the
#              OpenSSL helper, which requires a valid client certificate,
#              captures the forwarded payload.
#   error-key  the key URI names a missing object and carries the PIN; the
#              oracle is rsyslog's error naming the key with the PIN masked,
#              and the PIN appearing nowhere in rsyslog's own messages.
# GnuTLS finds the token through p11-kit, so the test registers SoftHSM in a
# per-test HOME. It skips cleanly when SoftHSM, pkcs11-tool, GnuTLS p11tool or
# GnuTLS PKCS#11 support is unavailable.
. ${srcdir:=.}/diag.sh init

check_command_available openssl
check_command_available softhsm2-util
check_command_available pkcs11-tool
check_command_available p11tool

if [ ! -x ./openssl_mtls_server ]; then
	echo "openssl_mtls_server helper not built - skipping test"
	exit 77
fi
gtls_module="$(cd .. && pwd)/runtime/.libs/lmnsd_gtls.so"
if [ ! -f "$gtls_module" ]; then
	echo "lmnsd_gtls module not built - skipping test"
	exit 77
fi
if ! grep -q '^#define HAVE_GNUTLS_PKCS11_PRIVKEY_IMPORT_URL 1' ../config.h; then
	echo "GnuTLS without PKCS#11 support - skipping test"
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

softhsm_module=$(find_first_existing \
	/usr/lib64/pkcs11/libsofthsm2.so \
	/usr/lib/pkcs11/libsofthsm2.so \
	/usr/lib64/softhsm/libsofthsm2.so \
	/usr/lib/softhsm/libsofthsm2.so \
	/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so \
	/usr/lib/aarch64-linux-gnu/softhsm/libsofthsm2.so \
	/usr/lib64/libsofthsm2.so \
	/usr/lib/libsofthsm2.so \
	/opt/homebrew/lib/softhsm/libsofthsm2.so \
	/usr/local/lib/softhsm/libsofthsm2.so) || {
	printf 'info: skipping test - SoftHSM PKCS#11 module not found\n'
	skip_test
}

workdir="$(pwd)/$RSYSLOG_DYNNAME.omfwd-tls-gtls-pkcs11-$GTLS_PKCS11_CASE"
port_file="$workdir/server.port"
server_capture="$workdir/server.capture"
server_stderr="$workdir/server.stderr"
softhsm_conf="$workdir/softhsm2.conf"
token_dir="$workdir/tokens"
token_label="rsyslog-gtls-test-token"
token_pin="482915"
token_so_pin="73926184"
started_log="$RSYSLOG_DYNNAME.started"
mkdir -p "$workdir" "$token_dir" "$workdir/home/.config/pkcs11/modules"

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
openssl x509 -in "$workdir/client.pem" -outform DER -out "$workdir/client.der" || error_exit 1
openssl pkey -in "$workdir/client.key" -pubout -outform DER -out "$workdir/client.pub.der" || \
	error_exit 1

cat >"$softhsm_conf" <<EOF
directories.tokendir = $token_dir
objectstore.backend = file
slots.removable = false
EOF
export SOFTHSM2_CONF="$softhsm_conf"
# p11-kit reads its user module directory from HOME; this keeps the token private to the test.
printf 'module: %s\n' "$softhsm_module" >"$workdir/home/.config/pkcs11/modules/softhsm.module"
export HOME="$workdir/home"

softhsm2-util --init-token --free --label "$token_label" --so-pin "$token_so_pin" --pin "$token_pin" || error_exit 1
softhsm2-util --import "$workdir/client.key" --token "$token_label" --label client-key --id 01 --pin "$token_pin" || error_exit 1
pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/client.pub.der" --type pubkey --id 01 --label client-pub || error_exit 1
pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/client.der" --type cert --id 01 --label client-cert || error_exit 1
# A pkcs11: CAFile trusts the certificates the token marks as trusted, which needs the SO login.
GNUTLS_SO_PIN="$token_so_pin" p11tool --login --so-login --mark-trusted --mark-ca --write \
	--load-certificate "$workdir/ca.pem" --label ca-cert "pkcs11:token=$token_label" || error_exit 1

ca_uri="pkcs11:token=$token_label;object=ca-cert;type=cert"
cert_uri="pkcs11:token=$token_label;object=client-cert;type=cert"
key_uri="pkcs11:token=$token_label;object=client-key;type=private;pin-value=$token_pin"
target_port=1
if [ "$GTLS_PKCS11_CASE" = "error-key" ]; then
	key_uri="pkcs11:token=$token_label;object=missing-key;type=private;pin-value=$token_pin"
else
	./openssl_mtls_server 0 "$workdir/server.pem" "$workdir/server.key" "$workdir/ca.pem" \
		"$port_file" "$server_capture" 2>"$server_stderr" &
	server_pid=$!
	wait_file_exists_for_process "$port_file" "$server_pid" 200 "OpenSSL MTLS helper" "$server_stderr"
	target_port=$(cat "$port_file")
fi

generate_conf
add_conf '
global(
	compatibility.defaults.secure="strict"
	net.ipprotocol="ipv4-only"
)

module(load="'$gtls_module'")
template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "omfwd-gtls-pkcs11" then {
	action(
		type="omfwd"
		target="127.0.0.1"
		protocol="tcp"
		port="'$target_port'"
		template="outfmt"
		StreamDriver="gtls"
		StreamDriverMode="1"
		StreamDriverAuthMode="x509/certvalid"
		streamdriver.cafile="'$ca_uri'"
		streamdriver.certfile="'$cert_uri'"
		streamdriver.keyfile="'$key_uri'"
	)
}
'

startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - omfwd-gtls-pkcs11'
if [ "$GTLS_PKCS11_CASE" = "error-key" ]; then
	wait_content "error adding our certificate" "$started_log"
	shutdown_immediate
	wait_shutdown
	content_check "object=missing-key;type=private;pin-value=******" "$started_log"
	check_not_present "$token_pin" "$started_log"
else
	wait_file_lines "$server_capture" 1
	shutdown_when_empty
	wait_shutdown
	wait $server_pid || {
		cat "$server_stderr"
		error_exit 1
	}
	content_count_check --regex '^omfwd-gtls-pkcs11$' 1 "$server_capture"
	check_not_present "$token_pin" "$started_log"
fi

exit_test
