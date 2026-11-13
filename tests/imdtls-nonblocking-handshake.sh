#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A client that stops after the DTLS cookie exchange must not stall the listener.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=1
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
assign_file_content port "$port_file"

python3 "$srcdir/imdtls-two-flight-proxy.py" "$port" > "$RSYSLOG_DYNNAME.proxy.log" 2>&1 &
proxy_pid=$!
trap 'kill "$proxy_pid" "$attacker_pid" "$honest_pid" 2>/dev/null || true' EXIT
for i in $(seq 1 100); do
    grep -q '^PORT ' "$RSYSLOG_DYNNAME.proxy.log" && break
    sleep 0.1
done
proxy_port=$(awk '/^PORT / {print $2; exit}' "$RSYSLOG_DYNNAME.proxy.log")
[ -n "$proxy_port" ] || error_exit 1

( sleep 15 | openssl s_client -dtls1_2 -connect "127.0.0.1:$proxy_port" -quiet \
    -cert "$srcdir/tls-certs/cert.pem" -key "$srcdir/tls-certs/key.pem" \
    -CAfile "$srcdir/tls-certs/ca.pem" > "$RSYSLOG_DYNNAME.attacker.log" 2>&1 ) &
attacker_pid=$!
for i in $(seq 1 100); do
    grep -q '^FORWARD 2 ' "$RSYSLOG_DYNNAME.proxy.log" && break
    sleep 0.1
done
grep -q '^FORWARD 2 ' "$RSYSLOG_DYNNAME.proxy.log" || error_exit 1

./tcpflood -b1 -W1000 -p"$port" -m1 -Tdtls \
    -x"$srcdir/tls-certs/ca.pem" -Z"$srcdir/tls-certs/cert.pem" \
    -z"$srcdir/tls-certs/key.pem" -L0 > "$RSYSLOG_DYNNAME.honest.log" 2>&1 &
honest_pid=$!
sleep 7
content_check 'msgnum:00000000'

kill "$proxy_pid" "$attacker_pid" "$honest_pid" 2>/dev/null || true
shutdown_when_empty
wait_shutdown
exit_test
