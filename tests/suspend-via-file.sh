#!/bin/bash
# This tests the action suspension via a file; we use a SUSPENDED string
# with trailing whitespace in this test.
# This file is part of the rsyslog project, released under ASL 2.0
# Written 2018-08-13 by Rainer Gerhards
. ${srcdir:=.}/diag.sh init
check_q_empty_log1() {
	echo "###### in check1"
	wait_seq_check 2500 4999
}
check_q_empty_log2() {
	echo "###### in check2"
	wait_seq_check 0 $LOG2_EXPECTED_LASTNUM
}
generate_conf
add_conf '
/* Filter out busy debug output, comment out if needed */
global( debug.whitelist="on"
	debug.files=["ruleset.c", "../action.c", "omfwd.c"]
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

:msg, contains, "msgnum:" {
	action(name="primary" type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="outfmt"
		action.externalstate.file="'$RSYSLOG_DYNNAME'.STATE" action.resumeinterval="1")
	action(name="failover" type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
		action.execOnlyWhenPreviousIsSuspended="on")
}
'
startup

printf '\n%s %s\n' "$(tb_timestamp)" \
	'STEP 1: checking that action is active w/o external state file'
injectmsg 0 2500
export SEQ_CHECK_FILE="$RSYSLOG2_OUT_LOG"
export LOG2_EXPECTED_LASTNUM=2499
export QUEUE_EMPTY_CHECK_FUNC=check_q_empty_log2
wait_queueempty
seq_check 0 $LOG2_EXPECTED_LASTNUM # full correctness check


printf '\n%s %s\n' "$(tb_timestamp)" \
	'STEP 2: checking that action becomes suspended via external state file'
printf 'SUSPENDED \n' > $RSYSLOG_DYNNAME.STATE
injectmsg 2500 2500
export SEQ_CHECK_FILE="$RSYSLOG_OUT_LOG"
export QUEUE_EMPTY_CHECK_FUNC=check_q_empty_log1
wait_queueempty
seq_check 2500 4999 # full correctness check

printf '\n%s %s\n' "$(tb_timestamp)" \
	'STEP 3: checking that action becomes resumed again via external state file'
printf "%s" "READY" > $RSYSLOG_DYNNAME.STATE
./msleep 2000 # ensure ResumeInterval expired (NOT sensitive to slow machines --> absolute time!)
injectmsg 2500 2500
export SEQ_CHECK_FILE="$RSYSLOG2_OUT_LOG"
export LOG2_EXPECTED_LASTNUM=4999
export QUEUE_EMPTY_CHECK_FUNC=check_q_empty_log2
shutdown_when_empty
wait_shutdown

# final checks
export SEQ_CHECK_FILE="$RSYSLOG_OUT_LOG"
seq_check 2500 4999
export SEQ_CHECK_FILE="$RSYSLOG2_OUT_LOG"
seq_check 0 4999
exit_test
