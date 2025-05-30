#!/bin/bash
# add 2019-02-26 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/omrabbitmq/.libs/omrabbitmq")
action(type="omrabbitmq" exchange="in" routing_key_template="jk" host="host")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown
content_check "template 'jk' used for routing key does not exist"

exit_test