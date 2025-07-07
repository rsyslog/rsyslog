#!/bin/bash
# add 2019-08-05 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
action(type="omusrmsg")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown
content_check "parameter 'users' required but not specified"
exit_test
