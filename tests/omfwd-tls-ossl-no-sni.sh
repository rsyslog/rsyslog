#!/bin/bash
# Author: John Cantu
# Test that no TLS SNI is sent by omfwd when connecting to an IP address

. ${srcdir:=.}/diag.sh init

port_file="$RSYSLOG_DYNNAME.sni-server.port"
omfwd_sni_server "openssl" "$port_file"
port=$(cat "$port_file")

generate_conf
add_conf '
global(DefaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'" \
       net.ipprotocol="ipv4-only")

action(type="omfwd"
        target="127.0.0.1"
        protocol="tcp"
        port="'$port'"
        StreamDriver="ossl"
        StreamDriverMode="1"
        StreamDriverAuthMode="anon"
        )
'

startup
omfwd_sni_check "(NULL)"
shutdown_immediate
wait_shutdown
kill -9 $(cat "$RSYSLOG_DYNNAME.sni-server.pid")
exit_test
