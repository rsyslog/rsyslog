#!/bin/bash
# added 2018-12-18 by ludobrands
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
test_status unreliable 'https://github.com/rsyslog/rsyslog/issues/3197'
check_command_available kafkacat

export KEEP_KAFKA_RUNNING="YES"
export TESTMESSAGES=100000
export TESTMESSAGESFULL=$TESTMESSAGES

export RANDTOPIC=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 8 | head -n 1)

# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka
echo Check and Stop previous instances of kafka/zookeeper 
download_kafka
stop_zookeeper
stop_kafka

echo Create kafka/zookeeper instance and $RANDTOPIC topic
start_zookeeper
start_kafka
create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

# --- Create/Start omkafka sender config 
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
# impstats in order to gain insight into error cases
module(load="../plugins/impstats/.libs/impstats"
	log.file="'$RSYSLOG_DYNNAME.pstats'"
	interval="1" log.syslog="off")
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")
$imdiagInjectDelayMode full

module(load="../plugins/omkafka/.libs/omkafka")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

template(name="keyin" type="list"){
  property(name="$.inkey")
}
  
local4.* {
	set $.inkey = substring(field($msg,":",2),7,1);
 	action(	name="kafka-fwd"
	type="omkafka"
	topic="'$RANDTOPIC'"
	key="keyin"
	dynaKey="on"
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
	action( type="omfile" file="'$RSYSLOG_OUT_LOG'")
	stop
}

action( type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'")
'

echo Starting sender instance [omkafka]
startup

echo Inject messages into rsyslog sender instance  
injectmsg 1 $TESTMESSAGES

wait_file_lines $RSYSLOG_OUT_LOG $TESTMESSAGESFULL 100

# experimental: wait until kafkacat receives everything

timeoutend=100
timecounter=0

while [ $timecounter -lt $timeoutend ]; do
	(( timecounter++ ))

	kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%s' > $RSYSLOG_OUT_LOG
	count=$(wc -l < ${RSYSLOG_OUT_LOG})
	if [ $count -eq $TESTMESSAGESFULL ]; then
		printf '**** wait-kafka-lines success, have %d lines ****\n\n' "$TESTMESSAGESFULL"
	        kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%p %k\n' | sort | uniq > "$RSYSLOG_OUT_LOG.extra"
        	count=$(wc -l < "${RSYSLOG_OUT_LOG}.extra")
	        if [ $count -eq 10 ]; then
			printf '**** partition check success, have 10 partition-key combinations ****\n\n' 
			break
	        else
			shutdown_when_empty
			wait_shutdown
			printf '\n\nERROR: partition check failed, expected 10 got %s\n' "$count"
			printf '\ņRAW DATA:\n'
			kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%p %k\n'
			printf '\nCHECKED OUTPUT:\n'
			cat "$RSYSLOG_OUT_LOG.extra"
			error_exit 1
	        fi
	else
		if [ "x$timecounter" == "x$timeoutend" ]; then
			echo wait-kafka-lines failed, expected $TESTMESSAGESFULL got $count
			shutdown_when_empty
			wait_shutdown
			error_exit 1
		else
			echo wait-file-lines not yet there, currently $count lines
			printf 'pstats data:\n'
			# we use tail below to guard against overwhelming the
			# logs if things go wild
			tail -n 500 < $RSYSLOG_DYNNAME.pstats
			printf '\n'

			$TESTTOOL_DIR/msleep 1000
		fi
	fi
echo end iteration  $timecounter
done
unset count

#end experimental

echo Stopping sender instance [omkafka]
shutdown_when_empty
wait_shutdown

#kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%s' > $RSYSLOG_OUT_LOG
#kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%p@%o:%k:%s' > $RSYSLOG_OUT_LOG.extra

# Delete topic to remove old traces before
delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

# Dump Kafka log | uncomment if needed
# dump_kafka_serverlog

kafka_check_broken_broker $RSYSLOG_DYNNAME.othermsg
seq_check 1 $TESTMESSAGESFULL -d

exit_test

