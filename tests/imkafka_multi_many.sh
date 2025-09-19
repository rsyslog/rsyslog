#!/bin/bash
# added 2018-08-29 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
echo Init Testbench
. ${srcdir:=.}/diag.sh init
check_command_available kcat

# *** ==============================================================================
export TESTMESSAGES=100000
export TESTMESSAGESFULL=100000

# Generate random topic name
export RANDTOPIC="$(printf '%08x' "$(( (RANDOM<<16) ^ RANDOM ))")"

# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafkamulti
echo ===============================================================================
echo Check and Stop previous instances of kafka/zookeeper
download_kafka
stop_zookeeper '.dep_wrk1'
stop_zookeeper '.dep_wrk2'
stop_zookeeper '.dep_wrk3'
stop_kafka '.dep_wrk1'
stop_kafka '.dep_wrk2'
stop_kafka '.dep_wrk3'

echo Create kafka/zookeeper instance and $RANDTOPIC topic
start_zookeeper '.dep_wrk1'
start_zookeeper '.dep_wrk2'
start_zookeeper '.dep_wrk3'
start_kafka '.dep_wrk1'
start_kafka '.dep_wrk2'
start_kafka '.dep_wrk3'

wait_for_kafka_startup '.dep_wrk1'
wait_for_kafka_startup '.dep_wrk2'
wait_for_kafka_startup '.dep_wrk3'

# create new topic
create_kafka_topic $RANDTOPIC '.dep_wrk1' '22181'

# --- Create imkafka receiver config
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka"
	topic="'$RANDTOPIC'"
	broker=["127.0.0.1:29092", "127.0.0.1:29093", "127.0.0.1:29094"]
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
	broker=["127.0.0.1:29092", "127.0.0.1:29093", "127.0.0.1:29094"]
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
	broker=["127.0.0.1:29092", "127.0.0.1:29093", "127.0.0.1:29094"]
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
	broker=["127.0.0.1:29092", "127.0.0.1:29093", "127.0.0.1:29094"]
	consumergroup="default4"
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

# Start imkafka receiver config
echo Starting receiver instance [imkafka]
startup
# ---

# Measure Starttime
TIMESTART=$(date +%s.%N)

# --- Fill Kafka Server with messages
# Can properly be done in a better way?!
for i in {00000001..00100000}
do
	echo " msgnum:$i" >> $RSYSLOG_OUT_LOG.in
done

echo Inject messages into kafka
kcat <$RSYSLOG_OUT_LOG.in  -P -b 127.0.0.1:29092 -t $RANDTOPIC
# ---

echo Ensuring kafka brokers remain reachable before shutdown...
wait_for_kafka_startup '.dep_wrk1'
wait_for_kafka_startup '.dep_wrk2'
wait_for_kafka_startup '.dep_wrk3'

echo Stopping sender instance [omkafka]
shutdown_when_empty
wait_shutdown

# Measure Endtime
TIMEEND=$(date +%s.%N)
TIMEDIFF=$(echo "$TIMEEND - $TIMESTART" | bc)
echo "*** imkafka time to process all data: $TIMEDIFF seconds!"

# Delete topic to remove old traces before
delete_kafka_topic $RANDTOPIC '.dep_wrk1' '22181'

# Do the final sequence check
seq_check 1 $TESTMESSAGESFULL -d

echo success
exit_test
