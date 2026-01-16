#!/bin/bash
# Test checks that ratelimit.name is mutually exclusive with ratelimit.interval
# and ratelimit.name is mutually exclusive with ratelimit.burst
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
ratelimit(name="testcheck" interval="10" burst="100")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" ratelimit.name="testcheck" ratelimit.interval="10")
'
startup
shutdown_when_empty
wait_shutdown
content_check "ratelimit.name is mutually exclusive with ratelimit.interval and ratelimit.burst" $RSYSLOG_DYNNAME.started

# Test imptcp exclusivity
# We can't run two instances easily in one test script with just diag.sh init, so we usually restart or use separate conf?
# Let's do a second run for imptcp.


generate_conf
add_conf '
ratelimit(name="testcheck" interval="10" burst="100")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" ratelimit.name="testcheck" ratelimit.burst="100")
'
startup
shutdown_when_empty
wait_shutdown
content_check "ratelimit.name is mutually exclusive with ratelimit.interval and ratelimit.burst" $RSYSLOG_DYNNAME.started
exit_test
