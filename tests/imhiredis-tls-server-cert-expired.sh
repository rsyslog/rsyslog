#!/usr/bin/env bash
# added 2025-11-03 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
#export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

start_redis
set_redis_tls_expired

redis_command "RPUSH mykey message1"
redis_command "RPUSH mykey message2"
redis_command "RPUSH mykey message3"

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/imhiredis/.libs/imhiredis")
template(name="outfmt" type="string" string="%$/num% %msg%\n")


input(type="imhiredis"
        server="127.0.0.1"
        port="'$REDIS_RANDOM_TLS_PORT'"
        use_tls="on"
        ca_cert_bundle="'$srcdir/testsuites/x.509/ca.pem'"
        client_cert="'$srcdir/testsuites/x.509/machine-cert.pem'"
        client_key="'$srcdir/testsuites/x.509/machine-key.pem'"
        key="mykey"
        mode="queue"
        ruleset="redis")

ruleset(name="redis") {
  set $/num = cnum($/num + 1);
  action(type="omfile"
            file="'$RSYSLOG_OUT_LOG'"
            template="outfmt")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown

stop_redis

content_check 'error while initializing TLS context: SSL_connect failed'

check_not_present '1 message3'
check_not_present '2 message2'
check_not_present '3 message1'

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
