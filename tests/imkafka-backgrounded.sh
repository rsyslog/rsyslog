#!/bin/bash
# added 2018-10-24 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kafkacat

export TESTMESSAGES=100000
export RANDTOPIC=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 8 | head -n 1)
# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
#export EXTRA_EXITCHECK=dumpkafkalogs
#export EXTRA_EXIT=kafka

echo Check and Stop previous instances of kafka/zookeeper
download_kafka
stop_zookeeper
stop_kafka

echo Create kafka/zookeeper instance and $RANDTOPIC topic
start_zookeeper
start_kafka

# create new topic
create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

# --- Create imkafka receiver config
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
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

export RSTB_DAEMONIZE="YES"
startup

for i in {00000001..00100000}; do
	echo " msgnum:$i" >> $RSYSLOG_DYNNAME.in
done

echo Inject messages into kafka
kafkacat -P -b localhost:29092 -t $RANDTOPIC < $RSYSLOG_DYNNAME.in

echo Give imkafka some time to start...
sleep 5

echo Stopping sender instance [omkafka]
shutdown_when_empty
wait_shutdown

# Delete topic to remove old traces before
delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

seq_check 1 $TESTMESSAGES -d
exit_test
