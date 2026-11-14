#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A delayed post-cookie DTLS flight must still enforce permitted-peer checks.
# Debug output only synchronizes the release with the server's WANT_READ state;
# the configured omfile output is the acceptance/rejection oracle. Bounded
# 0.1-second polls wait for proxy readiness, WANT_READ (up to 75 seconds), and
# client/server completion (up to 20 seconds), without assuming fixed latency.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=1
export RSYSLOG_DEBUG='debug nostdout'
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debug.log"

attacker_key="$RSYSLOG_DYNNAME.attacker.key"
attacker_csr="$RSYSLOG_DYNNAME.attacker.csr"
attacker_cert="$RSYSLOG_DYNNAME.attacker.pem"
openssl req -newkey rsa:2048 -nodes -keyout "$attacker_key" -out "$attacker_csr" \
    -subj '/CN=not-permitted.example' >/dev/null 2>&1 || error_exit 1
openssl x509 -req -in "$attacker_csr" -CA "$srcdir/tls-certs/ca.pem" \
    -CAkey "$srcdir/tls-certs/ca-key.pem" -CAserial "$RSYSLOG_DYNNAME.attacker.srl" \
    -CAcreateserial -out "$attacker_cert" -days 1 >/dev/null 2>&1 || error_exit 1

generate_conf
name_port_file="$RSYSLOG_DYNNAME.imdtls-name.port"
fingerprint_port_file="$RSYSLOG_DYNNAME.imdtls-fingerprint.port"
add_conf '
global(processInternalMessages="on")
module(load="../plugins/imdtls/.libs/imdtls")
input(type="imdtls" name="dtls-name" port="0" listenPortFileName="'$name_port_file'"
    timeout="8" tls.authmode="name" tls.permittedpeer="rsyslog-client"
    tls.cacert="'$srcdir'/tls-certs/ca.pem"
    tls.mycert="'$srcdir'/tls-certs/cert.pem" tls.myprivkey="'$srcdir'/tls-certs/key.pem")
input(type="imdtls" name="dtls-fingerprint" port="0" listenPortFileName="'$fingerprint_port_file'"
    timeout="8" tls.authmode="fingerprint"
    tls.permittedpeer="SHA1:5C:C6:62:D5:9D:25:9F:BC:F3:CB:61:FA:D2:B3:8B:61:88:D7:06:C3"
    tls.cacert="'$srcdir'/tls-certs/ca.pem"
    tls.mycert="'$srcdir'/tls-certs/cert.pem" tls.myprivkey="'$srcdir'/tls-certs/key.pem")
template(name="outfmt" type="string" string="%inputname%|%rawmsg%\n")
*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_file_content name_port "$name_port_file"
assign_file_content fingerprint_port "$fingerprint_port_file"

proxy_pid='' client_pid='' client_fd=''
cleanup_helpers() {
    [ -n "$client_fd" ] && exec {client_fd}>&-
    for pid in "$client_pid" "$proxy_pid"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
    done
    for pid in "$client_pid" "$proxy_pid"; do
        [ -n "$pid" ] && wait "$pid" 2>/dev/null || true
    done
    client_pid='' proxy_pid='' client_fd=''
}
trap cleanup_helpers EXIT

# Each case waits for its own new WANT_READ event. OpenSSL retries and slow CI
# may delay this transition; the 75-second bound remains below testbench guard.
run_case() {
    local auth="$1" peer="$2" port="$3" marker="$4"
    local prefix="$RSYSLOG_DYNNAME.$auth.$peer"
    local cert="$attacker_cert" key="$attacker_key"
    local baseline current reject_before reject_after proxy_port attempt finished=0 outcome=0
    [ "$peer" = allowed ] && { cert="$srcdir/tls-certs/cert.pem"; key="$srcdir/tls-certs/key.pem"; }
    baseline=$(grep -c "SSL_accept didn't complete" "$RSYSLOG_DEBUGLOG" 2>/dev/null || true)
    baseline=${baseline:-0}
    reject_before=$(grep -c 'Cert Verify FAILED' "$RSYSLOG_OUT_LOG" 2>/dev/null || true)
    reject_before=${reject_before:-0}
    python3 "$srcdir/imdtls-permittedpeer-pacing-proxy.py" "$port" "$prefix.release" 100 \
        >"$prefix.proxy.log" 2>&1 &
    proxy_pid=$!
    for ((attempt=0; attempt<100; attempt++)); do
        grep -q '^PORT ' "$prefix.proxy.log" 2>/dev/null && break
        kill -0 "$proxy_pid" 2>/dev/null || break
        sleep 0.1
    done
    proxy_port=$(awk '/^PORT / {print $2; exit}' "$prefix.proxy.log")
    [ -n "$proxy_port" ] || error_exit 1
    mkfifo "$prefix.client.fifo" || error_exit 1
    openssl s_client -dtls1_2 -connect "127.0.0.1:$proxy_port" -quiet -state \
        -cert "$cert" -key "$key" -CAfile "$srcdir/tls-certs/ca.pem" \
        <"$prefix.client.fifo" >"$prefix.client.out" 2>"$prefix.client.err" &
    client_pid=$!
    exec {client_fd}>"$prefix.client.fifo"
    printf '%s\n' "$marker" >&"$client_fd"
    for ((attempt=0; attempt<750; attempt++)); do
        current=$(grep -c "SSL_accept didn't complete" "$RSYSLOG_DEBUGLOG" 2>/dev/null || true)
        [ "${current:-0}" -gt "$baseline" ] && break
        kill -0 "$client_pid" 2>/dev/null || break
        sleep 0.1
    done
    [ "${current:-0}" -gt "$baseline" ] || { echo "$auth/$peer: no WANT_READ"; error_exit 1; }
    touch "$prefix.release"
    for ((attempt=0; attempt<200; attempt++)); do
        grep -Eq 'SSL_connect:.*read finished|SSL_connect:SSL negotiation finished successfully' \
            "$prefix.client.err" && finished=1
        if [ "$peer" = denied ]; then
            reject_after=$(grep -c 'Cert Verify FAILED' "$RSYSLOG_OUT_LOG" 2>/dev/null || true)
            [ "${reject_after:-0}" -gt "$reject_before" ] && outcome=1
        else
            grep -qF "$marker" "$RSYSLOG_OUT_LOG" 2>/dev/null && outcome=1
        fi
        [ "$finished" -eq 1 ] && [ "$outcome" -eq 1 ] && break
        sleep 0.1
    done
    [ "$finished" -eq 1 ] && [ "$outcome" -eq 1 ] || {
        echo "$auth/$peer: incomplete handshake or missing expected server outcome"
        tail -n 20 "$prefix.proxy.log" "$prefix.client.err" "$RSYSLOG_OUT_LOG" 2>/dev/null
        error_exit 1
    }
    cleanup_helpers
    rm -f "$prefix.client.fifo"
}

run_case name denied "$name_port" 'R3-NAME-DENIED'
run_case name allowed "$name_port" 'R3-NAME-ALLOWED'
run_case fingerprint denied "$fingerprint_port" 'R3-FINGERPRINT-DENIED'
run_case fingerprint allowed "$fingerprint_port" 'R3-FINGERPRINT-ALLOWED'

shutdown_when_empty
wait_shutdown
content_check 'Cert Verify FAILED'
content_check 'R3-NAME-ALLOWED'
content_check 'R3-FINGERPRINT-ALLOWED'
assert_content_missing 'R3-NAME-DENIED'
assert_content_missing 'R3-FINGERPRINT-DENIED'
exit_test
