#!/bin/bash
# Test named rate limits for omhttp (config-validation)
# Verifies that omhttp accepts ratelimit.name without error.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="omhttp_test_limit" interval="2" burst="5")

module(load="../contrib/omhttp/.libs/omhttp")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omhttp" server="localhost" serverport="19280"
       ratelimit.name="omhttp_test_limit"
       retryfailures="on" template="outfmt")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
