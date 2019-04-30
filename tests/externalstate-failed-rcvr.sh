#!/bin/bash
# This tests the action suspension via a file and ensure that it does
# NOT override actual target suspension state.
# This file is part of the rsyslog project, released under ASL 2.0
# Written 2019-01-08 by Rainer Gerhards
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=2500
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check

# IMPORTANT: nothing permitted to listen on $TCPFLOOD_PORT!
# mimic some good state in external statefile:
printf "%s" "READY" > $RSYSLOG_DYNNAME.STATE

generate_conf
add_conf '
$template outfmt,"%msg:F,58:2%\n"

:msg, contains, "msgnum:" {
	action(name="primary" type="omfwd" target="localhost" port="'$TCPFLOOD_PORT'"
		protocol="tcp" action.externalstate.file="'$RSYSLOG_DYNNAME'.STATE")
	action(name="failover" type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
		action.execOnlyWhenPreviousIsSuspended="on")
}
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
# note: we do NOT need to do a check_seq as this was already done as part of the
# queue empty predicate check
exit_test
