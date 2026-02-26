#!/bin/bash
# Test named rate limits for imhttp (config-validation)
# Verifies that imhttp accepts ratelimit.name without error.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="imhttp_test_limit" interval="2" burst="5")

module(load="../contrib/imhttp/.libs/imhttp"
       ports="0"
       listenportfilename="'$RSYSLOG_DYNNAME'.imhttp.port")
input(type="imhttp" endpoint="/test" ratelimit.name="imhttp_test_limit" ruleset="imhttp")

template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="imhttp") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
shutdown_when_empty
wait_shutdown
exit_test
