#!/bin/bash
# added 2019-04-11 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmnormalize/.libs/mmnormalize")

action(type="mmnormalize" rule=
	["rule=: %-:char-to{\"extradata\":\":\"}%:00000000:",
	 "rule=: %-:char-to{\"extradata\":\":\"}%:00000010:"] )

if not ($rawmsg contains "rsyslog") then
	if $parsesuccess == "OK" then
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
	else
		action(type="omfile" file="'$RSYSLOG_DYNNAME'.failed")
'
startup
injectmsg 0 20
shutdown_when_empty
wait_shutdown
content_check "msgnum:00000000:" $RSYSLOG_OUT_LOG
content_check "msgnum:00000010:" $RSYSLOG_OUT_LOG
content_check "msgnum:00000001:" $RSYSLOG_DYNNAME.failed
content_check "msgnum:00000012:" $RSYSLOG_DYNNAME.failed
exit_test
