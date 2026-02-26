#!/bin/bash
# Test named rate limits for imuxsock
# Verifies that imuxsock accepts ratelimit.name and
# syssock.ratelimit.name without error.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="imuxsock_test_limit" interval="2" burst="5")
ratelimit(name="imuxsock_syssock_limit" interval="3" burst="10")

module(load="../plugins/imuxsock/.libs/imuxsock"
       SysSock.Use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-imuxsock.sock"
      ratelimit.name="imuxsock_test_limit")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
