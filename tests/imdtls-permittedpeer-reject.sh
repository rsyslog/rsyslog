#!/bin/bash
# Verify that imdtls drops CA-authenticated peers that fail its permitted-peer check.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=1

attacker_key="$RSYSLOG_DYNNAME.attacker.key"
attacker_csr="$RSYSLOG_DYNNAME.attacker.csr"
attacker_cert="$RSYSLOG_DYNNAME.attacker.pem"
openssl req -newkey rsa:2048 -nodes -keyout "$attacker_key" -out "$attacker_csr" \
	-subj '/CN=not-permitted.example' >/dev/null 2>&1
openssl x509 -req -in "$attacker_csr" -CA "$srcdir/tls-certs/ca.pem" \
	-CAkey "$srcdir/tls-certs/ca-key.pem" -CAserial "$RSYSLOG_DYNNAME.attacker.srl" \
	-CAcreateserial -out "$attacker_cert" \
	-days 1 >/dev/null 2>&1

generate_conf
name_port_file="$RSYSLOG_DYNNAME.imdtls-name.port"
fingerprint_port_file="$RSYSLOG_DYNNAME.imdtls-fingerprint.port"
name_port=''
fingerprint_port=''
add_conf '
module(load="../plugins/imdtls/.libs/imdtls")

input(type="imdtls" port="0" listenPortFileName="'$name_port_file'"
	tls.authmode="name" tls.permittedpeer="rsyslog"
	tls.cacert="'$srcdir'/tls-certs/ca.pem"
	tls.mycert="'$srcdir'/tls-certs/cert.pem"
	tls.myprivkey="'$srcdir'/tls-certs/key.pem")

input(type="imdtls" port="0" listenPortFileName="'$fingerprint_port_file'"
	tls.authmode="fingerprint"
	tls.permittedpeer="SHA1:5C:C6:62:D5:9D:25:9F:BC:F3:CB:61:FA:D2:B3:8B:61:88:D7:06:C3"
	tls.cacert="'$srcdir'/tls-certs/ca.pem"
	tls.mycert="'$srcdir'/tls-certs/cert.pem"
	tls.myprivkey="'$srcdir'/tls-certs/key.pem")

*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_file_content name_port "$name_port_file"
assign_file_content fingerprint_port "$fingerprint_port_file"

tcpflood -b1 -W1000 -p"$name_port" -m1 -Tdtls -x"$srcdir/tls-certs/ca.pem" \
	-Z"$attacker_cert" -z"$attacker_key" -L0
tcpflood -b1 -W1000 -p"$fingerprint_port" -m1 -Tdtls -x"$srcdir/tls-certs/ca.pem" \
	-Z"$attacker_cert" -z"$attacker_key" -L0

sleep 2
shutdown_when_empty
wait_shutdown
content_check 'Cert Verify FAILED'
assert_content_missing 'msgnum:00000000'
exit_test
