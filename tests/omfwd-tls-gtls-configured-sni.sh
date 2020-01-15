#!/bin/bash
# Author: John Cantu
# Test that "custom-sni" is sent as SNI by omfwd, when configured

. ${srcdir:=.}/diag.sh init

port=$(get_free_port)

generate_conf
add_conf '
global(DefaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'" \
       net.ipprotocol="ipv4-only")

action(type="omfwd"
        target="localhost"
        protocol="tcp"
        port="'$port'"
        StreamDriver="gtls"
        StreamDriverMode="1"
        StreamDriverAuthMode="anon"
        StreamDriverRemoteSNI="custom-sni"
        )
'
omfwd_sni_server "gnutls" "$port"
startup
omfwd_sni_check "custom-sni"
shutdown_immediate
wait_shutdown
kill -9 $(cat "$RSYSLOG_DYNNAME.sni-server.pid")
exit_test
