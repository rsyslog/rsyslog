#!/bin/bash
# added 2019-11-14 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" queue.mindequeuebatchsize="20")
'
startup
shutdown_when_empty
wait_shutdown
content_check "queue is in direct mode, but parameters have been set"
exit_test
