#!/bin/bash
# This tests the action suspension via a file
# This file is part of the rsyslog project, released under ASL 2.0
# Written 2019-07-10 by Rainer Gerhards
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10000
#export NUMMESSAGES=100 #00
generate_conf
add_conf '
/* Filter out busy debug output, comment out if needed */
global( debug.whitelist="on"
	debug.files=["ruleset.c", "../action.c", "omfwd.c"]
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

:msg, contains, "msgnum:" {
	action(name="forwarder" type="omfwd" template="outfmt"
		target="127.0.0.1" port="'$TCPFLOOD_PORT'" protocol="tcp"
		action.externalstate.file="'$RSYSLOG_DYNNAME'.STATE"
		action.resumeRetryCount="-1" action.resumeinterval="1")
}
'

./minitcpsrv -t127.0.0.1 -p$TCPFLOOD_PORT -f $RSYSLOG_OUT_LOG &
BGPROCESS=$!
echo background minitcpsrv process id is $BGPROCESS

startup
injectmsg 0 5000
#injectmsg 0 5

printf '\n%s %s\n' "$(tb_timestamp)" \
	'checking that action becomes suspended via external state file'
printf "%s" "SUSPENDED" > $RSYSLOG_DYNNAME.STATE
./msleep 2000 # ensure ResumeInterval expired (NOT sensitive to slow machines --> absolute time!)
injectmsg 5000 1000
#injectmsg 5 5

printf '\n%s %s\n' "$(tb_timestamp)" \
	'checking that action becomes resumed again via external state file'
./msleep 2000 # ensure ResumeInterval expired (NOT sensitive to slow machines --> absolute time!)
printf "%s" "READY" > $RSYSLOG_DYNNAME.STATE

injectmsg 6000 4000
#export QUEUE_EMPTY_CHECK_FUNC=check_q_empty_log2
wait_queueempty
seq_check
shutdown_when_empty
wait_shutdown
exit_test
