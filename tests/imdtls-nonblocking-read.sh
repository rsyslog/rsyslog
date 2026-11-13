#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# An idle DTLS client must not prevent a second client from being served.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=2
selfsigned_key="$RSYSLOG_DYNNAME.selfsigned.key"
selfsigned_cert="$RSYSLOG_DYNNAME.selfsigned.pem"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$selfsigned_key" \
    -out "$selfsigned_cert" -subj '/CN=untrusted-local-client' -days 1 >/dev/null 2>&1

generate_conf
port_file="$RSYSLOG_DYNNAME.imdtls.port"
add_conf '
global(defaultNetstreamDriverCAFile="'$srcdir'/tls-certs/ca.pem"
       defaultNetstreamDriverCertFile="'$srcdir'/tls-certs/cert.pem"
       defaultNetstreamDriverKeyFile="'$srcdir'/tls-certs/key.pem")
module(load="../plugins/imdtls/.libs/imdtls")
input(type="imdtls" port="0" listenPortFileName="'$port_file'" timeout="15")
*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
port=''
assign_file_content port "$port_file"

first_input="$RSYSLOG_DYNNAME.first-client.fifo"
second_input="$RSYSLOG_DYNNAME.second-client.fifo"
first_client_pid=''
second_client_pid=''
first_fd=''
second_fd=''
cleanup_clients() {
    local pid
    [ -n "$first_fd" ] && exec {first_fd}>&-
    [ -n "$second_fd" ] && exec {second_fd}>&-
    for pid in "$first_client_pid" "$second_client_pid"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
    done
    for pid in "$first_client_pid" "$second_client_pid"; do
        [ -n "$pid" ] && wait "$pid" 2>/dev/null || true
    done
    first_client_pid='' second_client_pid='' first_fd='' second_fd=''
    rm -f "$first_input" "$second_input"
}
trap cleanup_clients EXIT
mkfifo "$first_input" "$second_input"
openssl s_client -dtls1_2 -connect "127.0.0.1:$port" -quiet \
    -cert "$selfsigned_cert" -key "$selfsigned_key" \
    -CAfile "$srcdir/tls-certs/ca.pem" < "$first_input" > "$RSYSLOG_DYNNAME.first-client.log" 2>&1 &
first_client_pid=$!
exec {first_fd}> "$first_input"
sleep 1
printf '<13>Nov 13 09:00:00 firsthost first-message\n' >&"$first_fd"

for ((attempt = 0; attempt < 200; ++attempt)); do
    grep -q 'first-message' "$RSYSLOG_OUT_LOG" 2>/dev/null && break
    sleep 0.1
done
content_check 'first-message'

openssl s_client -dtls1_2 -connect "127.0.0.1:$port" -quiet \
    -cert "$srcdir/tls-certs/cert.pem" -key "$srcdir/tls-certs/key.pem" \
    -CAfile "$srcdir/tls-certs/ca.pem" < "$second_input" > "$RSYSLOG_DYNNAME.second-client.log" 2>&1 &
second_client_pid=$!
exec {second_fd}> "$second_input"
sleep 1
printf '<13>Nov 13 09:00:00 secondhost second-message\n' >&"$second_fd"

sleep 7
content_check 'second-message'
cleanup_clients
shutdown_when_empty
wait_shutdown
exit_test
