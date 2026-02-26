#!/bin/bash
# Test named rate limits for imjournal (config-validation)
# Verifies that imjournal accepts ratelimit.name without error.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="imjournal_test_limit" interval="2" burst="5")

module(load="../plugins/imjournal/.libs/imjournal"
       ratelimit.name="imjournal_test_limit"
       StateFile="'$RSYSLOG_DYNNAME'.imjournal.state"
       IgnorePreviousMessages="on")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
