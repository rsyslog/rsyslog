#!/bin/bash
# Test for imkafka JSON message splitting with empty records array
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=1
export TESTMESSAGESFULL=$TESTMESSAGES
# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka

export RANDTOPIC="$(printf '%08x' "$(( (RANDOM<<16) ^ RANDOM ))")"

download_kafka
stop_zookeeper
stop_kafka

start_zookeeper
start_kafka
wait_for_kafka_startup
create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server with JSON splitting enabled */
input(	type="imkafka"
	topic="'$RANDTOPIC'"
	broker="127.0.0.1:29092"
	consumergroup="default"
	split.json.records="on"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

action( type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
startup

# Inject a JSON batch message with empty records array
printf '%s\n' '{"records":[]}' | kcat -P -b localhost:29092 -t $RANDTOPIC

shutdown_when_empty
wait_shutdown

delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

# Check that the empty array message was forwarded as-is (not split, since there are no records)
content_check '{"records":[]}'

exit_test
