#!/bin/bash
# added 2018-08-29 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"
# False positive codefactor.io
export RSYSLOG_OUT_LOG_1="${RSYSLOG_OUT_LOG:-default}.1"
export RSYSLOG_OUT_LOG_2="${RSYSLOG_OUT_LOG:-default}.2"

export TESTMESSAGES=100000
# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
#export EXTRA_EXITCHECK=dumpkafkalogs
#export EXTRA_EXIT=kafka

download_kafka
stop_zookeeper
stop_kafka
start_zookeeper
start_kafka

export RANDTOPIC="$(printf '%08x' "$(( (RANDOM<<16) ^ RANDOM ))")"

create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

# Create FIRST rsyslog instance
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="rsysloggroup"
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
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG_1` template="outfmt" )
}
'
echo Starting first rsyslog instance [imkafka]
startup

# Create SECOND rsyslog instance
export RSYSLOG_DEBUGLOG="log2"
generate_conf 2
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka" 
	topic="'$RANDTOPIC'" 
	broker="localhost:29092"
	consumergroup="rsysloggroup"
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
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG_2` template="outfmt" )
}
' 2
echo Starting second rsyslog instance [imkafka]
startup 2


TIMESTART=$(date +%s.%N)

injectmsg_kcat
# special case: number of test messages differs from file output
wait_file_lines $RSYSLOG_OUT_LOG $((TESTMESSAGES)) ${RETRIES:-200}
# Check that at least 25% messages are in both logfiles, otherwise load balancing hasn't worked
wait_file_lines $RSYSLOG_OUT_LOG_1 $((TESTMESSAGES/4)) ${RETRIES:-200}
wait_file_lines $RSYSLOG_OUT_LOG_2 $((TESTMESSAGES/4)) ${RETRIES:-200}

echo Stopping first rsyslog instance [imkafka]
shutdown_when_empty
wait_shutdown

echo Stopping second rsyslog instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

TIMEEND=$(date +%s.%N)
TIMEDIFF=$(echo "$TIMEEND - $TIMESTART" | bc)
echo "*** imkafka time to process all data: $TIMEDIFF seconds!"

delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

seq_check 1 $TESTMESSAGES -d

exit_test
