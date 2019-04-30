#!/bin/bash
# add 2019-04-15 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omudpspoof/.libs/omudpspoof")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
action(type="omudpspoof")
'
startup
shutdown_when_empty
wait_shutdown
content_check "parameter 'target' required but not specified"

exit_test
