#!/bin/bash
# Test named rate limits for imklog (config-validation)
# Verifies that imklog accepts ratelimit.name without error.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="imklog_test_limit" interval="2" burst="5")

module(load="../plugins/imklog/.libs/imklog"
       ratelimit.name="imklog_test_limit")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
