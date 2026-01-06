#!/bin/bash
# Test for duplicate named rate limits
# This should FAIL with a configuration error
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="test_limit" interval="2" burst="5")
ratelimit(name="test_limit" interval="1" burst="10")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
# Redirect rsyslog stderr to a file we can grep

# Expect startup to log an error
startup

# We don't expect it to run correctly if it fails config, but lets check the log
shutdown_when_empty
wait_shutdown
content_check "ratelimit: duplicate name 'test_limit' in current config set"
exit_test