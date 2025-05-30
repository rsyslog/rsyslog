#!/bin/bash
# add 2019-02-26 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/omrabbitmq/.libs/omrabbitmq")
action(type="omrabbitmq" exchange="in" routing_key="jk" host="host" expiration="-1")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown
content_check "parameter 'expiration' cannot be less than zero"

exit_test
