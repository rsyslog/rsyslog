#!/bin/bash
# Test named rate limits for omrelp (config-validation).
# Verifies that omrelp accepts ratelimit.name without startup errors.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
ratelimit(name="omrelp_test_limit" interval="2" burst="5")

module(load="../plugins/omrelp/.libs/omrelp")
action(type="omrelp" target="127.0.0.1" port="2514"
       ratelimit.name="omrelp_test_limit")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
