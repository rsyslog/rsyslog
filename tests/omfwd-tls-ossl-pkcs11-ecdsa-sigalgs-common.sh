#!/bin/bash
# Common positive ECDSA matrix for PKCS#11-backed omfwd mTLS signing tests.
. ${srcdir:=.}/diag.sh init

case "$(basename "$0")" in
omfwd-tls-ossl-pkcs11-ecdsa-sigalgs-common.sh)
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
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-key-mtls-ecdsa-sigalgs"
		TEST_CA_REF="$workdir/ca.pem"
		TEST_CERT_REF="$workdir/client.pem"
		TEST_KEY_REF="$key_uri"
		;;
	cert_key)
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-cert-mtls-ecdsa-sigalgs"
		TEST_CA_REF="$workdir/ca.pem"
		TEST_CERT_REF="$cert_uri"
		TEST_KEY_REF="$key_uri"
		;;
	ca_key)
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-ca-mtls-ecdsa-sigalgs"
		TEST_CA_REF="$ca_uri"
		TEST_CERT_REF="$workdir/client.pem"
		TEST_KEY_REF="$key_uri"
		;;
	full)
		TEST_MSG_PREFIX="omfwd-ossl-pkcs11-mtls-ecdsa-sigalgs"
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
workdir="$RSYSLOG_DYNNAME.${PKCS11_SIGALG_VARIANT}.ecdsa"
softhsm_conf="$workdir/softhsm2.conf"
openssl_conf="$workdir/openssl-pkcs11.cnf"
token_dir="$workdir/tokens"
setup_log="$workdir/setup.log"
mkdir -p "$workdir" "$token_dir"
rm -f "$setup_log"

run_setup_cmd() {
	"$@" >>"$setup_log" 2>&1 || {
		printf 'setup command failed:'
		printf ' %s' "$@" >&2
		printf '\n' >&2
		cat "$setup_log" >&2
		error_exit 1
	}
}

skip_with_setup_log() {
	printf '%s\n' "$1"
	if [ -f "$setup_log" ]; then
		cat "$setup_log"
	fi
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
keyUsage=digitalSignature
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

run_setup_cmd openssl genrsa -out "$workdir/ca.key" 3072
run_setup_cmd openssl req -new -key "$workdir/ca.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=Test-CA" \
	-out "$workdir/ca.csr"
run_setup_cmd openssl x509 -req -in "$workdir/ca.csr" -signkey "$workdir/ca.key" -sha256 -days 365 \
	-extfile "$workdir/ca.cnf" -extensions v3_ca \
	-out "$workdir/ca.pem"

run_setup_cmd openssl genrsa -out "$workdir/server.key" 3072
run_setup_cmd openssl req -new -key "$workdir/server.key" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=localhost" \
	-out "$workdir/server.csr"
run_setup_cmd openssl x509 -req -in "$workdir/server.csr" -CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/server.ext" \
	-out "$workdir/server.pem"

run_setup_cmd openssl x509 -in "$workdir/ca.pem" -outform DER -out "$workdir/ca.der"

cat >"$softhsm_conf" <<EOF
directories.tokendir = $token_dir
objectstore.backend = file
slots.removable = false
EOF
export SOFTHSM2_CONF="$softhsm_conf"

run_setup_cmd softhsm2-util --init-token --free --label "$token_label" --so-pin "$token_so_pin" --pin "$token_pin"

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

run_setup_cmd pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--keypairgen --key-type EC:prime256v1 --id 01 --label client-key --usage-sign

run_setup_cmd pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--read-object --type pubkey --id 01 --output-file "$workdir/client.pub.der"
run_setup_cmd openssl pkey -pubin -inform DER -in "$workdir/client.pub.der" -out "$workdir/client.pub.pem"
run_setup_cmd openssl x509 -new \
	-force_pubkey "$workdir/client.pub.pem" \
	-subj "/C=US/ST=CA/L=Test/O=rsyslog/OU=test/CN=rsyslog-client-ecdsa" \
	-CA "$workdir/ca.pem" -CAkey "$workdir/ca.key" \
	-CAcreateserial -sha256 -days 365 -extfile "$workdir/client.ext" \
	-out "$workdir/client.pem"
run_setup_cmd openssl x509 -in "$workdir/client.pem" -outform DER -out "$workdir/client.der"

run_setup_cmd pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/client.der" --type cert --id 01 --label client-cert
run_setup_cmd pkcs11-tool --module "$softhsm_module" --login --pin "$token_pin" --token-label "$token_label" \
	--write-object "$workdir/ca.der" --type cert --id 02 --label ca-cert

printf 'rsyslog pkcs11 ecdsa sign probe\n' >"$workdir/ecdsa-probe.txt"
if ! openssl dgst -sha256 -sign "$key_uri" -out "$workdir/ecdsa-probe.sig" \
	"$workdir/ecdsa-probe.txt" >>"$setup_log" 2>&1; then
	skip_with_setup_log "info: skipping test - OpenSSL pkcs11 provider EVP ECDSA signing failed, though token setup succeeded"
fi

setup_variant

run_case() {
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
	printf 'running %s case: %s\n' "$TEST_MSG_PREFIX" "$case_name"

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
	wait_file_lines "$server_capture" 1
	shutdown_when_empty
	wait_shutdown

	wait $server_pid || {
		cat "$server_stderr"
		error_exit 1
	}

	content_count_check --regex "^${msg}\$" 1 "$server_capture"
}

run_case \
	"tls12-ecdsa-sha256" \
	"MinProtocol=TLSv1.2
MaxProtocol=TLSv1.2
ClientSignatureAlgorithms=ECDSA+SHA256" \
	"MinProtocol=TLSv1.2
MaxProtocol=TLSv1.2
ClientSignatureAlgorithms=ECDSA+SHA256"

run_case \
	"tls13-ecdsa-secp256r1-sha256" \
	"MinProtocol=TLSv1.3
MaxProtocol=TLSv1.3
ClientSignatureAlgorithms=ecdsa_secp256r1_sha256" \
	"MinProtocol=TLSv1.3
MaxProtocol=TLSv1.3
ClientSignatureAlgorithms=ecdsa_secp256r1_sha256"

exit_test
