#!/bin/bash
# added 2017-06-06 by alorbach
#	This tests the keepFailedMessages feature in omkafka
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=50000
export TESTMESSAGESFULL=50000

# Generate random topic name
export RANDTOPIC=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

echo ===============================================================================
echo \[sndrcv_kafka_failresume.sh\]: Create kafka/zookeeper instance and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic $RANDTOPIC '.dep_wrk' '22181'

echo \[sndrcv_kafka_failresume.sh\]: Give Kafka some time to process topic create ...
sleep 5

echo \[sndrcv_kafka_failresume.sh\]: Init Testbench
. $srcdir/diag.sh init

# --- Create omkafka receiver config
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka"
	topic="'$RANDTOPIC'"
	broker="localhost:29092"
	consumergroup="default"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if ($msg contains "msgnum:") then {
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt" )
}
'

echo \[sndrcv_kafka_failresume.sh\]: Starting receiver instance [omkafka]
startup
# ---

# --- Create omkafka sender config
export RSYSLOG_DEBUGLOG="log2"
generate_conf 2
add_conf '
main_queue(queue.timeoutactioncompletion="10000" queue.timeoutshutdown="60000")

module(load="../plugins/omkafka/.libs/omkafka")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="'$TCPFLOOD_PORT'")	/* this port for tcpflood! */

template(name="outfmt" type="string" string="%msg%\n")

action(	name="kafka-fwd"
	type="omkafka"
	topic="'$RANDTOPIC'"
	broker="localhost:29092"
	template="outfmt"
	confParam=[	"compression.codec=none",
			"socket.timeout.ms=5000",
			"socket.keepalive.enable=true",
			"reconnect.backoff.jitter.ms=1000",
			"queue.buffering.max.messages=20000",
			"message.send.max.retries=1"]
	topicConfParam=["message.timeout.ms=5000"]
	partitions.auto="on"
	resubmitOnFailure="on"
	keepFailedMessages="on"
	failedMsgFile="omkafka-failed.data"
	action.resumeInterval="2"
	action.resumeRetryCount="10"
	queue.saveonshutdown="on"
	)
' 2

echo \[sndrcv_kafka_failresume.sh\]: Starting sender instance [imkafka]
startup 2
# ---
>>>>>>> testbench: Added new kafka tests using kafkacat for better debugging.

echo \[sndrcv_kafka_failresume.sh\]: Inject messages into rsyslog sender instance
tcpflood -m$TESTMESSAGES -i1

echo \[sndrcv_kafka_failresume.sh\]: Stopping kafka cluster instance
. $srcdir/diag.sh stop-kafka

echo \[sndrcv_kafka_failresume.sh\]: Stopping sender instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka_failresume.sh\]: Starting kafka cluster instance
. $srcdir/diag.sh start-kafka

echo \[sndrcv_kafka_failresume.sh\]: Sleep to give rsyslog instances time to process data ...
sleep 5

echo \[sndrcv_kafka_failresume.sh\]: Starting sender instance [imkafka]
export RSYSLOG_DEBUGLOG="log3"
startup 2

echo \[sndrcv_kafka_failresume.sh\]: Sleep to give rsyslog sender time to send data ...
sleep 5

echo \[sndrcv_kafka_failresume.sh\]: Stopping sender instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka_failresume.sh\]: Stopping receiver instance [omkafka]
shutdown_when_empty
wait_shutdown

echo \[sndrcv_kafka_failresume.sh\]: delete kafka topics
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk' '22181'

echo \[sndrcv_kafka_failresume.sh\]: stop kafka instance
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper

# Do the final sequence check
seq_check 1 $TESTMESSAGESFULL

echo success
exit_test