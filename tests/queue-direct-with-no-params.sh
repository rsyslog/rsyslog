#!/bin/bash
# added 2019-11-14 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
check_not_present "queue is in direct mode, but parameters have been set"
exit_test
