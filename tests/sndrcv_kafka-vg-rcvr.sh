#!/bin/bash
# added 2017-05-03 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=100000
# enable the EXTRA_EXITCHECK only if really needed - otherwise spams the test log
# too much
export EXTRA_EXITCHECK=dumpkafkalogs
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk' '22181'

echo Give Kafka some time to process topic create ...
sleep 5

echo Starting receiver instance [omkafka]
export RSYSLOG_DEBUGLOG="log"
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka" 
	topic="static" 
	broker="localhost:29092" 
	consumergroup="default"
	confParam=[ "compression.codec=none",
		"socket.timeout.ms=1000",
		"socket.keepalive.enable=true"]
	)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if ($msg contains "msgnum:") then {
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt" )
}
'
startup_vg
. $srcdir/diag.sh wait-startup

echo Starting sender instance [imkafka]
export RSYSLOG_DEBUGLOG="log2"
generate_conf 2
add_conf '
main_queue(queue.timeoutactioncompletion="10000" queue.timeoutshutdown="60000")

module(load="../plugins/omkafka/.libs/omkafka")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")	/* this port for tcpflood! */

template(name="outfmt" type="string" string="%msg%\n")

action(	name="kafka-fwd" 
	type="omkafka" 
	topic="static" 
	broker="localhost:29092" 
	template="outfmt" 
	confParam=[	"compression.codec=none",
			"socket.timeout.ms=1000",
			"socket.keepalive.enable=true",
			"reconnect.backoff.jitter.ms=1000",
			"queue.buffering.max.messages=20000",
			"message.send.max.retries=1"]
	topicConfParam=["message.timeout.ms=1000"]
	partitions.auto="on"
	resubmitOnFailure="on"
	keepFailedMessages="on"
	failedMsgFile="omkafka-failed.data"
	action.resumeInterval="2"
	action.resumeRetryCount="10"
	queue.saveonshutdown="on"
	)
' 2
startup 2
. $srcdir/diag.sh wait-startup 2

echo Inject messages into rsyslog sender instance  
tcpflood -m$TESTMESSAGES -i1

echo Sleep to give rsyslog instances time to process data ...
sleep 5

echo Stopping sender instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo Sleep to give rsyslog receiver time to receive data ...
sleep 5

echo Stopping receiver instance [omkafka]
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg

# Do the final sequence check
seq_check 1 $TESTMESSAGES -d

echo stop kafka instance
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk' '22181'
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper

echo success
exit_test
