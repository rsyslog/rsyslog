#!/bin/bash
# added 2017-05-03 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
check_command_available kafkacat

# *** ==============================================================================
export TESTMESSAGES=10000
export TESTMESSAGESFULL=$TESTMESSAGES

# Generate random topic name
export RANDTOPIC=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 8 | head -n 1)

# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka
echo ===============================================================================
echo Check and Stop previous instances of kafka/zookeeper 
download_kafka
stop_zookeeper
stop_kafka

echo Create kafka/zookeeper instance and $RANDTOPIC topic
start_zookeeper
start_kafka
# create new topic
create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

# --- Create/Start omkafka sender config 
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")
$imdiagInjectDelayMode full

module(load="../plugins/omkafka/.libs/omkafka")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

local4.* {
	action(	name="kafka-fwd" 
	type="omkafka"
	topic="'$RANDTOPIC'"
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
	failedMsgFile="'$RSYSLOG_OUT_LOG'-failed-'$RANDTOPIC'.data"
	action.resumeInterval="1"
	action.resumeRetryCount="2"
	queue.saveonshutdown="on"
	)
	stop
}

action( type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'")
'

echo Starting sender instance [omkafka]
startup_vg
# --- 

injectmsg 1 $TESTMESSAGES

echo Stopping sender instance [omkafka]
shutdown_when_empty
wait_shutdown_vg
check_exit_vg

kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%s'> $RSYSLOG_OUT_LOG
kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%p@%o:%k:%s' > $RSYSLOG_OUT_LOG.extra

# Delete topic to remove old traces before
delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

kafka_check_broken_broker $RSYSLOG_DYNNAME.othermsg
seq_check 1 $TESTMESSAGESFULL -d

echo success
exit_test
