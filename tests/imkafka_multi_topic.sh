#!/bin/bash
# Test that a single imkafka input() can consume from multiple topics
# specified as an array: topic=["topic1", "topic2"]
# added 2026-05-29 by contributor
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=1000
export TESTMESSAGESFULL=$TESTMESSAGES
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka

RANDTOPIC1="$(printf '%08x' "$(( (RANDOM<<16) ^ RANDOM ))")"
RANDTOPIC2="$(printf '%08x' "$(( (RANDOM<<16) ^ RANDOM ))")"
export RANDTOPIC1 RANDTOPIC2

download_kafka
stop_zookeeper
stop_kafka

start_zookeeper
start_kafka
wait_for_kafka_startup
create_kafka_topic $RANDTOPIC1 '.dep_wrk' '22181'
create_kafka_topic $RANDTOPIC2 '.dep_wrk' '22181'

generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
input(	type="imkafka"
	topic=["'$RANDTOPIC1'", "'$RANDTOPIC2'"]
	broker="127.0.0.1:29092"
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
startup

HALF=$((TESTMESSAGES / 2))

# Inject first half of messages into topic 1
i=1
while (( i <= HALF )); do
	printf ' msgnum:%8.8d\n' $i
	i=$((i + 1))
done | kcat -P -b 127.0.0.1:29092 -t $RANDTOPIC1

# Inject second half of messages into topic 2
i=$((HALF + 1))
while (( i <= TESTMESSAGES )); do
	printf ' msgnum:%8.8d\n' $i
	i=$((i + 1))
done | kcat -P -b 127.0.0.1:29092 -t $RANDTOPIC2

wait_file_lines $RSYSLOG_OUT_LOG $TESTMESSAGES ${RETRIES:-200}
shutdown_when_empty
wait_shutdown

delete_kafka_topic $RANDTOPIC1 '.dep_wrk' '22181'
delete_kafka_topic $RANDTOPIC2 '.dep_wrk' '22181'

seq_check 1 $TESTMESSAGESFULL -d

exit_test
