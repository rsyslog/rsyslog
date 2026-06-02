#!/bin/bash
# Copyright (C) 2011 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test does not work on FreeBSD - problems with os utility option switches"
skip_ARM "disk-assisted queue receiver restore timing is flaky on ARM CI"
MAINQ_SEGMENT="${RSYSLOG_DYNNAME}.spool/mainq.00000001"

sender_mainqueuesize() {
	response="$(echo getmainmsgqueuesize | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2)" || error_exit $?
	echo "$response" | awk '{print $NF}'
}

mainq_segment_size() {
	if [ -f "$MAINQ_SEGMENT" ]; then
		stat -c%s "$MAINQ_SEGMENT"
	else
		echo 0
	fi
}

wait_sender_queues_outage_data() {
	initial_segment_size="$1"
	echo "waiting for sender queue and DA segment growth from $initial_segment_size bytes"
	i=0
	while [ $i -le $TB_TIMEOUT_STARTSTOP ]; do
		qsize="$(sender_mainqueuesize)"
		segment_size="$(mainq_segment_size)"
		if [ "${qsize:-0}" -gt 0 ] 2>/dev/null && [ "${segment_size:-0}" -gt "${initial_segment_size:-0}" ]; then
			echo "sender queue size $qsize, DA segment size $segment_size"
			return
		fi
		$TESTTOOL_DIR/msleep 100
		i=$((i + 1))
	done
	echo "FAIL: sender did not queue outage data before receiver restart"
	echo "last sender queue size: $qsize"
	echo "last DA segment size: $segment_size, expected > $initial_segment_size"
	ls -l ${RSYSLOG_DYNNAME}.spool
	error_exit 1
}
#
# STEP1: start both instances and send 1000 messages.
# Note: receiver is instance 1, sender instance 2.
#
# start up the instances. Note that the environment settings can be changed to
# set instance-specific debugging parameters!
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log2"
echo starting receiver
generate_conf
add_conf '
# then SENDER sends to this port (not tcpflood!)
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" ./'$RSYSLOG_OUT_LOG';outfmt
'
startup
export PORT_RCVR="$TCPFLOOD_PORT"
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log"
#valgrind="valgrind"
echo starting sender
generate_conf 2
add_conf '
$WorkDirectory '$RSYSLOG_DYNNAME'.spool
$MainMsgQueueSize 2000
$MainMsgQueueLowWaterMark 800
$MainMsgQueueHighWaterMark 1000
$MainMsgQueueDequeueBatchSize 1
$MainMsgQueueMaxFileSize 1g
$MainMsgQueueWorkerThreads 1
$MainMsgQueueFileName mainq

# we use the shortest resume interval a) to let the test not run too long 
# and b) make sure some retries happen before the reconnect
$ActionResumeInterval 1
$ActionSendResendLastMsgOnReconnect on
$ActionResumeRetryCount -1
*.*	@@127.0.0.1:'$PORT_RCVR'
' 2
startup 2
# re-set params so that new instances do not thrash it...
#unset RSYSLOG_DEBUG
#unset RSYSLOG_DEBUGLOG

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2  1 1000
wait_queueempty
./msleep 1000 # let things settle down a bit

#
# Step 2: shutdown receiver, then send some more data, which then
# needs to go into the queue.
#
echo step 2

shutdown_when_empty
wait_shutdown

INITIALFILESIZE="$(mainq_segment_size)"
injectmsg2  1001 10000
wait_sender_queues_outage_data "$INITIALFILESIZE"
get_mainqueuesize 2
ls -l ${RSYSLOG_DYNNAME}.spool

#
# Step 3: restart receiver, wait that the sender drains its queue
#
echo step 3
#export RSYSLOG_DEBUGLOG="log2"
generate_conf
add_conf '
# then SENDER sends to this port (not tcpflood!)
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="'$PORT_RCVR'")

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" ./'$RSYSLOG_OUT_LOG';outfmt
'
startup
echo waiting for sender to drain queue [may need a short while]
wait_queueempty 2
ls -l ${RSYSLOG_DYNNAME}.spool
OLDFILESIZE=$(stat -c%s ${RSYSLOG_DYNNAME}.spool/mainq.00000001)
echo file size to expect is $OLDFILESIZE


#
# Step 4: send new data. Queue files are not permitted to grow now
# (but one file continuous to exist).
#
echo step 4
injectmsg2  11001 10
wait_queueempty 2

# at this point, the queue file shall not have grown. Note
# that we MUST NOT shut down the instance right now, because it
# would clean up the queue files! So we need to do our checks
# first (here!).
ls -l ${RSYSLOG_DYNNAME}.spool
NEWFILESIZE=$(stat -c%s ${RSYSLOG_DYNNAME}.spool/mainq.00000001)
if [ $NEWFILESIZE != $OLDFILESIZE ]
then
   echo file sizes do not match, expected $OLDFILESIZE, actual $NEWFILESIZE
   echo this means that data has been written to the queue file where it
   echo no longer should be written.
   # abort will happen below, because we must ensure proper system shutdown
   # HOWEVER, during actual testing it may be useful to do an exit here (so
   # that e.g. the debug log is pointed right at the correct spot).
   # exit 1
fi

#
# We now do an extra test (so this is two in one ;)) to see if the DA
# queue can be reactivated after its initial shutdown. In essence, we 
# redo steps 2 and 3.
#
# Step 5: stop receiver again, then send some more data, which then
# needs to go into the queue.
#
echo step 5
echo "*** done primary test *** now checking if DA can be restarted"
shutdown_when_empty
wait_shutdown

INITIALFILESIZE="$(mainq_segment_size)"
injectmsg2  11011 10000
wait_sender_queues_outage_data "$INITIALFILESIZE"
get_mainqueuesize 2
ls -l ${RSYSLOG_DYNNAME}.spool

#
# Step 6: restart receiver, wait that the sender drains its queue
#
echo step 6
startup
echo waiting for sender to drain queue [may need a short while]
wait_queueempty 2
ls -l ${RSYSLOG_DYNNAME}.spool

#
# Queue file size checks done. Now it is time to terminate the system
# and see if everything could be received (the usual check, done here
# for completeness, more or less as a bonus).
#
shutdown_when_empty 2
wait_shutdown 2

# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

# now abort test if we need to (due to filesize predicate)
if [ $NEWFILESIZE != $OLDFILESIZE ]; then
	error_exit 1
fi
# do the final check
export SEQ_CHECK_OPTIONS=-d
seq_check 1 21010 -m 100
exit_test
