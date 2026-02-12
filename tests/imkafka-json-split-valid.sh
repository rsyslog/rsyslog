#!/bin/bash
# Test for imkafka JSON message splitting with valid records
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=2
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

# Inject a JSON batch message with two records
printf '%s\n' '{"records":[{"time":"2025-02-20T03:19:34.655Z","msg":"msgnum:00000001:"},{"time":"2025-02-20T03:19:34.693Z","msg":"msgnum:00000002:"}]}' | kcat -P -b localhost:29092 -t $RANDTOPIC

shutdown_when_empty
wait_shutdown

delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

# Check that we got 2 separate messages
content_count_check "msgnum:00000001:" 1
content_count_check "msgnum:00000002:" 1

# Verify that splitting actually occurred - the "records" wrapper should NOT be present
# This ensures the test fails if splitting doesn't work and the entire batch is forwarded as-is
content_check --regex '^[^"]*"records":' --invert

exit_test
