#!/bin/bash
# Test disk-assisted mode on the main message queue. The test uses a very small
# in-memory main queue so the 2000-message burst must spill to disk and then
# drain back into the configured omfile action. imdiag injection is marked fully
# delayable so the diagnostic injector does not overrun the intentionally tiny
# queue. The disk-assisted path can drain slowly under sanitizer/Valgrind CI, so
# the test allows up to four minutes per drain point and waits for the complete
# expected sequence instead of only a line count. The oracle is the complete
# output sequence 0..2099: first the pre-DA messages, then the DA-triggering
# burst, then a final small post-DA burst that proves normal processing resumed.
# added 2009-04-22 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export RSTB_IMDIAG_INJECT_DELAY_MODE=full
DA_MAINMSG_Q_TIMEOUT=${DA_MAINMSG_Q_TIMEOUT:-240}

wait_da_mainmsg_q_seq() {
	local endnum="$1"
	local timeout="${2:-$DA_MAINMSG_Q_TIMEOUT}"
	local timeoutend=$(( SECONDS + timeout ))
	local count=0

	while true ; do
		if seq_check --check-only 0 "$endnum" >/dev/null 2>&1 ; then
			printf 'wait_da_mainmsg_q_seq success, sequence 0..%s complete\n' "$endnum"
			return
		fi
		if [ -f "$RSYSLOG_OUT_LOG" ]; then
			count=$(wc -l < "$RSYSLOG_OUT_LOG")
		fi
		if [ "$SECONDS" -ge "$timeoutend" ]; then
			printf 'FAIL: wait_da_mainmsg_q_seq timed out waiting for sequence 0..%s, have %s lines\n' \
				"$endnum" "$count"
			seq_check 0 "$endnum"
			error_exit 1
		fi
		printf 'wait_da_mainmsg_q_seq waiting for sequence 0..%s, have %s lines\n' \
			"$endnum" "$count"
		"$TESTTOOL_DIR"/msleep 500
	done
}

da_mainmsg_q_complete() {
	seq_check --check-only 0 2099 >/dev/null 2>&1
}
export QUEUE_EMPTY_CHECK_FUNC=da_mainmsg_q_complete
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# set spool locations and switch queue to disk assisted mode
$WorkDirectory '$RSYSLOG_DYNNAME'.spool
$MainMsgQueueSize 200 # this *should* trigger moving on to DA mode...
# note: we must set QueueSize sufficiently high, so that 70% (light delay mark)
# is high enough above HighWatermark!
$MainMsgQueueHighWatermark 80
$MainMsgQueueLowWatermark 40
$MainMsgQueueFilename mainq
$MainMsgQueueType linkedlist

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup

# part1: send first 50 messages (in memory, only)
injectmsg 0 50
wait_da_mainmsg_q_seq 49 # let queue drain for this test case

# part 2: send bunch of messages. This should trigger DA mode
injectmsg 50 2000
ls -l ${RSYSLOG_DYNNAME}.spool	 # for manual review
wait_da_mainmsg_q_seq 2049 # wait to ensure DA queue is "empty"

# send another handful
injectmsg 2050 50
wait_da_mainmsg_q_seq 2099

shutdown_when_empty
wait_shutdown
seq_check  0 2099
exit_test
