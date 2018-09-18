#!/bin/bash
# added 2018-08-13 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
echo Init Testbench
. $srcdir/diag.sh init

# *** ==============================================================================
export TESTMESSAGES=50000
export TESTMESSAGESFULL=100000

# Generate random topic name
export RANDTOPIC1=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)
export RANDTOPIC2=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka
echo ===============================================================================
echo Check and Stop previous instances of kafka/zookeeper 
download_kafka
stop_zookeeper
stop_kafka

echo Create kafka/zookeeper instance and topics
start_zookeeper
start_kafka
create_kafka_topic $RANDTOPIC1 '.dep_wrk' '22181'
create_kafka_topic $RANDTOPIC2 '.dep_wrk' '22181'

# --- Create omkafka sender config
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")
$imdiagInjectDelayMode full

module(load="../plugins/omkafka/.libs/omkafka")

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
	failedMsgFile="'$RSYSLOG_OUT_LOG'-failed-'$RANDTOPIC1'.data"
	action.resumeInterval="1"
	action.resumeRetryCount="10"
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
	failedMsgFile="'$RSYSLOG_OUT_LOG'-failed-'$RANDTOPIC2'.data"
	action.resumeInterval="1"
	action.resumeRetryCount="10"
	queue.saveonshutdown="on"
	)
'

echo Starting sender instance [omkafka]
startup
# ---

# Injection messages now before starting receiver, simply because omkafka will take some time and
# there is no reason to wait for the receiver to startup first. 
echo Inject messages into rsyslog sender instance
injectmsg 1 $TESTMESSAGES

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

echo Starting receiver instance [imkafka]
startup 2
# ---

echo Stopping sender  instance [omkafka]
shutdown_when_empty
wait_shutdown

echo Stopping receiver instance [imkafka]
kafka_wait_group_coordinator
shutdown_when_empty 2
wait_shutdown 2

echo delete kafka topics
delete_kafka_topic $RANDTOPIC1 '.dep_wrk' '22181'
delete_kafka_topic $RANDTOPIC2 '.dep_wrk' '22181'

# Dump Kafka log | uncomment if needed
# dump_kafka_serverlog

# Do the final sequence check
seq_check 1 $TESTMESSAGES -d

linecount=$(wc -l < ${RSYSLOG_OUT_LOG})
if [ $linecount -ge $TESTMESSAGESFULL ]; then
	echo "Info: Count correct: $linecount"
else
	echo "Count error detected in $RSYSLOG_OUT_LOG"
	echo "number of lines in file: $linecount"
	error_exit 1 
fi

echo success
exit_test
