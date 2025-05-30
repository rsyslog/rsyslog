#!/bin/bash
# add 2020-01-08 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
action(type="omfwd" target="localhost" port="514" streamdriver.permitexpiredcerts="invld")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check "streamdriver.permitExpiredCerts must be 'warn', 'off' or 'on' but is 'invld'"
exit_test
