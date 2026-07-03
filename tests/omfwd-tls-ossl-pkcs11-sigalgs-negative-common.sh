#!/bin/bash
# Common negative matrix for PKCS#11-backed omfwd mTLS signing algorithm tests.
. ${srcdir:=.}/diag.sh init

case "$(basename "$0")" in
omfwd-tls-ossl-pkcs11-sigalgs-negative-common.sh)
	printf 'info: shared helper script; skipping direct execution\n'
	skip_test
	;;
esac

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

setup_variant() {
	case "${PKCS11_SIGALG_VARIANT}" in
	key_only)
		TEST_SUFFIX="key-mtls-sigalgs-negative"
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-key-mtls-sigalgs-negative"
		TEST_CA_REF="$workdir/ca.pem"
		TEST_CERT_REF="$workdir/client.pem"
		TEST_KEY_REF="$key_uri"
		;;
	cert_key)
		TEST_SUFFIX="cert-mtls-sigalgs-negative"
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-cert-mtls-sigalgs-negative"
		TEST_CA_REF="$workdir/ca.pem"
		TEST_CERT_REF="$cert_uri"
		TEST_KEY_REF="$key_uri"
		;;
	ca_key)
		TEST_SUFFIX="ca-mtls-sigalgs-negative"
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-ca-mtls-sigalgs-negative"
		TEST_CA_REF="$ca_uri"
		TEST_CERT_REF="$workdir/client.pem"
		TEST_KEY_REF="$key_uri"
		;;
	full)
		TEST_SUFFIX="mtls-sigalgs-negative"
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-mtls-sigalgs-negative"
		TEST_CA_REF="$ca_uri"
		TEST_CERT_REF="$cert_uri"
		TEST_KEY_REF="$key_uri"
		;;
	*)
		printf 'unknown PKCS11_SIGALG_VARIANT=%s\n' "${PKCS11_SIGALG_VARIANT}" >&2
		error_exit 1
		;;
	esac
}

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

token_label="rsyslog-test-token"
token_pin="123456"
token_so_pin="12345678"
workdir="$RSYSLOG_DYNNAME.${PKCS11_SIGALG_VARIANT}.negative"
softhsm_conf="$workdir/softhsm2.conf"
openssl_conf="$workdir/openssl-pkcs11.cnf"
token_dir="$workdir/tokens"
started_log="$RSYSLOG_DYNNAME.started"
mkdir -p "$workdir" "$token_dir"

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

openssl genrsa -out "$workdir/ca.key" 3072 || error_exit 1
openssl req -new -key "$workdir/ca.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=Test-CA" \
	-out "$workdir/ca.csr" || error_exit 1
openssl x509 -req -in "$workdir/ca.csr" -signkey "$workdir/ca.key" -sha256 -days 365 \
	-extfile "$workdir/ca.cnf" -extensions v3_ca \
	-out "$workdir/ca.pem" || error_exit 1

openssl genrsa -out "$workdir/server.key" 3072 || error_exit 1
openssl req -new -key "$workdir/server.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=localhost" \
	-out "$workdir/server.csr" || error_exit 1
openssl x509 -req -in "$workdir/server.csr" -CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/server.ext" \
	-out "$workdir/server.pem" || error_exit 1

openssl genrsa -out "$workdir/client.key" 3072 || error_exit 1
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

setup_variant

run_negative_case() {
	local case_name="$1"
	local client_cfg="$2"
	local server_cfg="$3"
	local msg="${TEST_MSG_PREFIX} ${case_name}"
	local port_file="$workdir/${case_name}.server.port"
	local server_capture="$workdir/${case_name}.server.capture"
	local server_stderr="$workdir/${case_name}.server.stderr"
	local server_pid=
	local server_port=

	rm -f "$port_file" "$server_capture" "$server_stderr"
	printf 'running negative %s case: %s\n' "$TEST_MSG_PREFIX" "$case_name"

	OPENSSL_MTLS_CFG="$server_cfg" \
	./openssl_mtls_server 0 "$workdir/server.pem" "$workdir/server.key" "$workdir/ca.pem" \
		"$port_file" "$server_capture" 2>"$server_stderr" &
	server_pid=$!
	wait_file_exists_for_process "$port_file" "$server_pid" 10 "OpenSSL MTLS helper" "$server_stderr"
	server_port=$(cat "$port_file")

	generate_conf
	add_conf "$(cat <<EOF
global(
	compatibility.defaults.secure="strict"
	net.ipprotocol="ipv4-only"
)

module(load="builtin:omfwd")
template(name="outfmt" type="string" string="%msg%\n")

if \$msg contains "$TEST_MSG_PREFIX" then {
	action(
		type="omfwd"
		target="127.0.0.1"
		protocol="tcp"
		port="$server_port"
		template="outfmt"
		StreamDriver="ossl"
		StreamDriverMode="1"
		StreamDriverAuthMode="x509/certvalid"
		streamdriver.cafile="$TEST_CA_REF"
		streamdriver.certfile="$TEST_CERT_REF"
		streamdriver.keyfile="$TEST_KEY_REF"
		gnutlsPriorityString="$client_cfg"
	)
}
EOF
)"

	startup
	injectmsg_literal "<165>1 2003-03-01T01:00:00.000Z host app - - - ${msg}"

	if content_check --check-only "SSL_ERROR_SYSCALL" "$started_log"; then
		:
	else
		wait_content "SSL_ERROR_SSL" "$started_log"
		content_check "SSL_ERROR_SSL" "$started_log"
		content_check "OpenSSL Error Stack:" "$started_log"
	fi

	shutdown_immediate
	wait_shutdown

	wait $server_pid || true
	content_check "OPENSSL-MTLS: SSL_accept failed" "$server_stderr"
	content_check "peer did not return a certificate" "$server_stderr"
	content_count_check --regex "^${msg}\$" 0 "$server_capture"
}

run_negative_case \
	"tls12-clientsig-ecdsa-only" \
	"MinProtocol=TLSv1.2
MaxProtocol=TLSv1.2" \
	"MinProtocol=TLSv1.2
MaxProtocol=TLSv1.2
ClientSignatureAlgorithms=ECDSA+SHA256"

run_negative_case \
	"tls13-clientsig-ecdsa-only" \
	"MinProtocol=TLSv1.3
MaxProtocol=TLSv1.3" \
	"MinProtocol=TLSv1.3
MaxProtocol=TLSv1.3
ClientSignatureAlgorithms=ecdsa_secp256r1_sha256"

exit_test
