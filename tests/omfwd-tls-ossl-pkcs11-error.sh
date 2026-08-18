#!/bin/bash
# Verify that omfwd with the ossl driver reports a deterministic error when an
# action-level TLS object is configured via a pkcs11: URI but the referenced
# PKCS#11 object does not exist. The test is parameterized by
# PKCS11_FAIL_KIND=ca|cert|key, provisions a temporary SoftHSM token with the
# other required TLS objects, injects one message to force the outbound connect
# path, and then waits for the expected rsyslog-internal startup/error log
# message in the testbench .started file.
. ${srcdir:=.}/diag.sh init

check_command_available openssl
check_command_available softhsm2-util
check_command_available pkcs11-tool

case "${PKCS11_FAIL_KIND:-}" in
ca)
	bad_uri_label="missing-ca"
	# The CA error now includes the failing pkcs11: URI, so match the stable
	# prefix here and assert the generic access-failure wording separately.
	expected_error="Error: CA certificate '"
	;;
cert)
	bad_uri_label="missing-cert"
	expected_error="Error: Certificate file could not be accessed"
	;;
key)
	bad_uri_label="missing-key"
	expected_error="Error: Key could not be accessed"
	;;
*)
	printf 'info: skipping helper test %s - PKCS11_FAIL_KIND not set (use ca, cert, or key)\n' "$0"
	skip_test
	;;
esac

find_first_existing() {
	for candidate in "$@"; do
		if [ -f "$candidate" ]; then
			printf '%s\n' "$candidate"
			return 0
		fi
	done
	return 1
}

workdir="$RSYSLOG_DYNNAME.omfwd-tls-ossl-pkcs11-error"
softhsm_conf="$workdir/softhsm2.conf"
openssl_conf="$workdir/openssl-pkcs11.cnf"
token_dir="$workdir/tokens"
token_label="rsyslog-test-token"
token_pin="123456"
token_so_pin="12345678"
started_log="$RSYSLOG_DYNNAME.started"
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

openssl genrsa -out "$workdir/client.key" 2048 || error_exit 1
openssl req -new -key "$workdir/client.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=rsyslog-client" \
	-out "$workdir/client.csr" || error_exit 1
openssl x509 -req -in "$workdir/client.csr" -CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/client.ext" \
	-out "$workdir/client.pem" || error_exit 1

openssl x509 -in "$workdir/ca.pem" -outform DER -out "$workdir/ca.der" || error_exit 1
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

case "$PKCS11_FAIL_KIND" in
ca)
	# Force CA loading to fail before any TCP connect is attempted.
	ca_uri="pkcs11:token=$token_label;object=$bad_uri_label;type=cert"
	;;
cert)
	# Force certificate-chain loading to fail while CA and key remain valid.
	cert_uri="pkcs11:token=$token_label;object=$bad_uri_label;type=cert"
	;;
key)
	# Force private-key loading to fail while CA and certificate remain valid.
	key_uri="pkcs11:token=$token_label;object=$bad_uri_label;type=private"
	;;
esac

generate_conf
add_conf '
global(
	compatibility.defaults.secure="strict"
	net.ipprotocol="ipv4-only"
)

module(load="builtin:omfwd")
template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "omfwd-ossl-pkcs11-error" then {
	action(
		type="omfwd"
		target="127.0.0.1"
		protocol="tcp"
		port="1"
		template="outfmt"
		StreamDriver="ossl"
		StreamDriverMode="1"
		StreamDriverAuthMode="x509/certvalid"
		streamdriver.cafile="'$ca_uri'"
		streamdriver.certfile="'$cert_uri'"
		streamdriver.keyfile="'$key_uri'"
	)
}
'

startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - omfwd-ossl-pkcs11-error'
wait_content "$expected_error" "$started_log"
shutdown_immediate
wait_shutdown
content_check "$expected_error" "$started_log"
if [ "$PKCS11_FAIL_KIND" = "ca" ]; then
	content_check "could not be accessed" "$started_log"
fi
content_check "OpenSSL Error Stack:" "$started_log"

exit_test
