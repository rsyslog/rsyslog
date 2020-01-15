#!/usr/bin/env bash
# Author: John Cantu
# Test that no TLS SNI is sent by omfwd when connecting to an IP address

. ${srcdir:=.}/diag.sh init

port=$(get_free_port)

generate_conf
add_conf '
global(DefaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'" \
       net.ipprotocol="ipv4-only")

action(type="omfwd"
        target="127.0.0.1"
        protocol="tcp"
        port="'$port'"
        StreamDriver="gtls"
        StreamDriverMode="1"
        StreamDriverAuthMode="anon"
        )
'

omfwd_sni_server "gnutls" "$port"
startup
omfwd_sni_check "(NULL)"
shutdown_immediate
wait_shutdown
kill -9 $(cat "$RSYSLOG_DYNNAME.sni-server.pid")
exit_test
