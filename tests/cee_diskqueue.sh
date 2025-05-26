#!/bin/bash
# check if CEE properties are properly saved & restored to/from disk queue
# added 2012-09-19 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test probably does not work on all flavors of Solaris"
export NUMMESSAGES=5000
generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
template(name="outfmt" type="string" string="%$!usr!msg:F,58:2%\n")

set $!usr!msg = $msg;
if $msg contains "msgnum" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
	       queue.type="disk" queue.filename="rsyslog-act1")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown 
seq_check
exit_test
