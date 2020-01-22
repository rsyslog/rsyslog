#!/bin/bash
# Author: John Cantu
# Test that "localhost" is sent as SNI by omfwd, when connecting to a hostname and no SNI is configured

. ${srcdir:=.}/diag.sh init

port=$(get_free_port)

generate_conf
add_conf '
global(DefaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'")

action(type="omfwd"
        target="localhost"
        protocol="tcp"
        port="'$port'"
        StreamDriver="gtls"
        StreamDriverMode="1"
        StreamDriverAuthMode="anon"
        )
'

omfwd_sni_server "gnutls" "$port"
startup
omfwd_sni_check "localhost"
shutdown_immediate
kill -9 $(cat "$RSYSLOG_DYNNAME.sni-server.pid")
exit_test