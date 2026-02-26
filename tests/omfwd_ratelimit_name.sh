#!/bin/bash
# Test named rate limits for omfwd (config-validation)
# Verifies that omfwd accepts ratelimit.name without error.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
ratelimit(name="omfwd_test_limit" interval="2" burst="5")

module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.rcvr.port")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then
    action(type="omfwd" target="127.0.0.1" port="514" protocol="udp"
           ratelimit.name="omfwd_test_limit" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
