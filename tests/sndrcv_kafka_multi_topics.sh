#!/bin/bash
# added 2018-08-13 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=50000
export TESTMESSAGESFULL=100000

# Generate random topic name
export RANDTOPIC1=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)
export RANDTOPIC2=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

# enable the EXTRA_EXITCHECK only if really needed - otherwise spams the test log
# too much
#export EXTRA_EXITCHECK=dumpkafkalogs
echo ===============================================================================
echo \[sndrcv_kafka_multi_topics.sh\]: Create kafka/zookeeper instance and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic $RANDTOPIC1 '.dep_wrk' '22181'
. $srcdir/diag.sh create-kafka-topic $RANDTOPIC2 '.dep_wrk' '22181'

echo \[sndrcv_kafka_multi_topics.sh\]: Give Kafka some time to process topic create ...
sleep 5

echo \[sndrcv_kafka_fail.sh\]: Init Testbench
. $srcdir/diag.sh init

# --- Create omkafka sender config
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/omkafka/.libs/omkafka")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="'$TCPFLOOD_PORT'")	/* this port for tcpflood! */

template(name="outfmt" type="string" string="%msg%\n")

local4.* action(	name="kafka-fwd"
	type="omkafka"
	topic="'$RANDTOPIC1'"
	broker="localhost:29092"
	template="outfmt"
	confParam=[	"compression.codec=none",
			"socket.timeout.ms=10000",
			"socket.keepalive.enable=true",
			"reconnect.backoff.jitter.ms=1000",
			"queue.buffering.max.messages=10000",
			"enable.auto.commit=true",
			"message.send.max.retries=1"]
	topicConfParam=["message.timeout.ms=10000"]
	partitions.auto="on"
	closeTimeout="60000"
	resubmitOnFailure="on"
	keepFailedMessages="on"
	failedMsgFile="omkafka-failed1.data"
	action.resumeInterval="1"
	action.resumeRetryCount="2"
	queue.saveonshutdown="on"
	)
local4.* action(	name="kafka-fwd"
	type="omkafka"
	topic="'$RANDTOPIC2'"
	broker="localhost:29092"
	template="outfmt"
	confParam=[	"compression.codec=none",
			"socket.timeout.ms=10000",
			"socket.keepalive.enable=true",
			"reconnect.backoff.jitter.ms=1000",
			"queue.buffering.max.messages=10000",
			"enable.auto.commit=true",
			"message.send.max.retries=1"]
	topicConfParam=["message.timeout.ms=10000"]
	partitions.auto="on"
	closeTimeout="60000"
	resubmitOnFailure="on"
	keepFailedMessages="on"
	failedMsgFile="omkafka-failed2.data"
	action.resumeInterval="1"
	action.resumeRetryCount="2"
	queue.saveonshutdown="on"
	)
'

echo \[sndrcv_kafka_multi_topics.sh\]: Starting sender instance [omkafka]
startup
# ---

echo \[sndrcv_kafka.sh\]: Inject messages into rsyslog sender instance
tcpflood -m$TESTMESSAGES -i1

# --- Create omkafka receiver config
export RSYSLOG_DEBUGLOG="log2"
generate_conf 2
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka"
	topic="'$RANDTOPIC1'"
	broker="localhost:29092"
	consumergroup="default1"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=10000",
		"enable.partition.eof=false",
		"reconnect.backoff.jitter.ms=1000",
		"socket.keepalive.enable=true"]
	)
input(	type="imkafka"
	topic="'$RANDTOPIC2'"
	broker="localhost:29092"
	consumergroup="default2"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=10000",
		"enable.partition.eof=false",
		"reconnect.backoff.jitter.ms=1000",
		"socket.keepalive.enable=true"]
	)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if ($msg contains "msgnum:") then {
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt" )
}
' 2

echo \[sndrcv_kafka_multi_topics.sh\]: Starting receiver instance [imkafka]
startup 2
# ---

echo \[sndrcv_kafka.sh\]: Stopping sender  instance [omkafka]
shutdown_when_empty
wait_shutdown

echo \[sndrcv_kafka.sh\]: Stopping receiver instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka.sh\]: delete kafka topics
. $srcdir/diag.sh delete-kafka-topic $RANDTOPIC1 '.dep_wrk' '22181'
. $srcdir/diag.sh delete-kafka-topic $RANDTOPIC2 '.dep_wrk' '22181'

echo \[sndrcv_kafka.sh\]: stop kafka instance
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper

# Do the final sequence check
seq_check 1 $TESTMESSAGES -d
content_check_with_count "000" $TESTMESSAGESFULL

echo success
exit_test