#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A client that stops after the DTLS cookie exchange must not stall the listener.
# A server flight after the second forwarded client datagram proves the
# stalled handshake exists before the honest client starts.
# Poll for honest-client delivery for at most 20 seconds (200 x 0.1 seconds).
# This permits CI scheduling and handshake delays while staying below the
# stalled session's 60-second timeout, so expiry cannot satisfy the oracle.
. ${srcdir:=.}/diag.sh init
check_command_available python3

export NUMMESSAGES=1
generate_conf
port_file="$RSYSLOG_DYNNAME.imdtls.port"
add_conf '
global(defaultNetstreamDriverCAFile="'$srcdir'/tls-certs/ca.pem"
       defaultNetstreamDriverCertFile="'$srcdir'/tls-certs/cert.pem"
       defaultNetstreamDriverKeyFile="'$srcdir'/tls-certs/key.pem")
module(load="../plugins/imdtls/.libs/imdtls")
input(type="imdtls" port="0" listenPortFileName="'$port_file'" timeout="60")
*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
port=''
assign_file_content port "$port_file"

attacker_input="$RSYSLOG_DYNNAME.attacker.fifo"
attacker_fd=''
proxy_pid=''
attacker_pid=''
honest_pid=''
honest_marker=''
cleanup_helpers() {
    local pid
    [ -n "$attacker_fd" ] && exec {attacker_fd}>&-
    for pid in "$proxy_pid" "$attacker_pid" "$honest_pid"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
    done
    for pid in "$proxy_pid" "$attacker_pid" "$honest_pid"; do
        [ -n "$pid" ] && wait "$pid" 2>/dev/null || true
    done
    proxy_pid='' attacker_pid='' honest_pid='' attacker_fd=''
    rm -f "$attacker_input"
}
trap cleanup_helpers EXIT
mkfifo "$attacker_input"
python3 "$srcdir/imdtls-two-flight-proxy.py" "$port" > "$RSYSLOG_DYNNAME.proxy.log" 2>&1 &
proxy_pid=$!
for ((attempt = 0; attempt < 100; ++attempt)); do
    grep -q '^PORT ' "$RSYSLOG_DYNNAME.proxy.log" && break
    sleep 0.1
done
proxy_port=$(awk '/^PORT / {print $2; exit}' "$RSYSLOG_DYNNAME.proxy.log")
[ -n "$proxy_port" ] || error_exit 1

openssl s_client -dtls1_2 -connect "127.0.0.1:$proxy_port" -quiet \
    -cert "$srcdir/tls-certs/cert.pem" -key "$srcdir/tls-certs/key.pem" \
    -CAfile "$srcdir/tls-certs/ca.pem" < "$attacker_input" > "$RSYSLOG_DYNNAME.attacker.log" 2>&1 &
attacker_pid=$!
exec {attacker_fd}> "$attacker_input"
for ((attempt = 0; attempt < 100; ++attempt)); do
    grep -q '^SERVER 2 ' "$RSYSLOG_DYNNAME.proxy.log" && break
    sleep 0.1
done
grep -q '^SERVER 2 ' "$RSYSLOG_DYNNAME.proxy.log" || error_exit 1

start_tcpflood_async honest_pid honest_marker -b1 -W1000 -p"$port" -m1 -Tdtls \
    -x"$srcdir/tls-certs/ca.pem" -Z"$srcdir/tls-certs/cert.pem" \
    -z"$srcdir/tls-certs/key.pem" -L0 > "$RSYSLOG_DYNNAME.honest.log" 2>&1
for ((attempt = 0; attempt < 200; ++attempt)); do
    grep -q 'msgnum:00000000' "$RSYSLOG_OUT_LOG" 2>/dev/null && break
    sleep 0.1
done
content_check 'msgnum:00000000'
wait_tcpflood_async "$honest_pid" "$honest_marker"
honest_pid=''

cleanup_helpers
shutdown_when_empty
wait_shutdown
exit_test
