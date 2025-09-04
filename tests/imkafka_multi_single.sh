#!/bin/bash
# added 2018-08-29 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=100000
# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
#export EXTRA_EXITCHECK=dumpkafkalogs
#export EXTRA_EXIT=kafka

download_kafka
stop_zookeeper
stop_kafka
start_zookeeper
start_kafka

export RANDTOPIC=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 8 | head -n 1)

create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default1"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default2"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default3"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default4"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default5"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default6"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default7"
	confParam=[ "compression.codec=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="default8"
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

TIMESTART=$(date +%s.%N)

injectmsg_kcat
# special case: number of test messages differs from file output
wait_file_lines $RSYSLOG_OUT_LOG $((TESTMESSAGES * 8)) ${RETRIES:-200}
shutdown_when_empty
wait_shutdown

TIMEEND=$(date +%s.%N)
TIMEDIFF=$(echo "$TIMEEND - $TIMESTART" | bc)
echo "*** imkafka time to process all data: $TIMEDIFF seconds!"

delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

seq_check 1 $TESTMESSAGES -d

exit_test
