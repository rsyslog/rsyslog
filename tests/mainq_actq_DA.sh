#!/bin/bash
# Test to check that mainq and actionq can be disk assisted without
# any problems. This was created to reproduce a segfault issue:
# https://github.com/rsyslog/rsyslog/issues/3681
# added 2019-06-03 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10	# we just need a handful - we primarily test startup
generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.fileName="main" queue.type="LinkedList")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:"
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
		queue.type="linkedList" queue.fileName="action")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown 
seq_check
exit_test
