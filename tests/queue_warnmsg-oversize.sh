#!/bin/bash
# add 2019-04-18 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'"
       queue.type="linkedList" queue.size="500001")
'

startup
shutdown_when_empty
wait_shutdown
content_check --regex "warning.*queue.size=500001 is very large"

exit_test
