#!/bin/bash
# added 2017-05-08 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=1000
export TESTMESSAGESFULL=1000

echo ===============================================================================
echo \[sndrcv_kafka_multi.sh\]: Create multiple kafka/zookeeper instances and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper '.dep_wrk1'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk2'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk3'
. $srcdir/diag.sh stop-kafka '.dep_wrk1'
. $srcdir/diag.sh stop-kafka '.dep_wrk2'
. $srcdir/diag.sh stop-kafka '.dep_wrk3'
. $srcdir/diag.sh start-zookeeper '.dep_wrk1'
. $srcdir/diag.sh start-zookeeper '.dep_wrk2'
. $srcdir/diag.sh start-zookeeper '.dep_wrk3'
. $srcdir/diag.sh start-kafka '.dep_wrk1'
. $srcdir/diag.sh start-kafka '.dep_wrk2'
. $srcdir/diag.sh start-kafka '.dep_wrk3'
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk1' '22181'

. $srcdir/diag.sh init

# --- Create omkafka receiver config 
generate_conf
add_conf '
module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka" 
	topic="static" 
	broker=["localhost:29092", "localhost:29093", "localhost:29094"] 
#	broker="localhost:29092" 
	consumergroup="default"
	confParam=[ "compression.codec=none",
		"socket.timeout.ms=5000",
		"enable.partition.eof=false",
		"socket.keepalive.enable=true"]
	)	

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if ($msg contains "msgnum:") then {
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt" )
}
'
# --- 

# --- Create omkafka sender config 
generate_conf 2
add_conf '
module(load="../plugins/omkafka/.libs/omkafka")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="'$TCPFLOOD_PORT'" Ruleset="omkafka")	/* this port for tcpflood! */

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="omkafka") {
	action( type="omkafka" 
		name="kafka-fwd" 
		broker=["localhost:29092", "localhost:29093", "localhost:29094"] 
		topic="static" 
		template="outfmt" 
		confParam=[	"compression.codec=none",
				"socket.timeout.ms=5000",
				"socket.keepalive.enable=true",
				"reconnect.backoff.jitter.ms=1000",
				"queue.buffering.max.messages=20000",
				"message.send.max.retries=1"]
		partitions.auto="on"
		partitions.scheme="random"
		queue.size="1000000"
		queue.type="LinkedList"
		action.repeatedmsgcontainsoriginalmsg="off"
		action.resumeRetryCount="-1"
		action.reportSuspension="on"
		action.reportSuspensionContinuation="on" )

}

ruleset(name="omkafka1") {
	action(name="kafka-fwd" type="omkafka" topic="static" broker="localhost:29092" template="outfmt" partitions.auto="on")
}
ruleset(name="omkafka2") {
	action(name="kafka-fwd" type="omkafka" topic="static" broker="localhost:29093" template="outfmt" partitions.auto="on")
}
ruleset(name="omkafka3") {
	action(name="kafka-fwd" type="omkafka" topic="static" broker="localhost:29094" template="outfmt" partitions.auto="on")
}
' 2
# --- 

echo \[sndrcv_kafka_multi.sh\]: Starting sender instance [omkafka]
export RSYSLOG_DEBUGLOG="log"
startup
. $srcdir/diag.sh wait-startup

# now inject the messages into instance 2. It will connect to instance 1, and that instance will record the data.
tcpflood -m$TESTMESSAGES -i1

echo \[sndrcv_kafka_multi.sh\]: Starting receiver instance [imkafka]
export RSYSLOG_DEBUGLOG="log2"
startup 2
. $srcdir/diag.sh wait-startup 2

#echo \[sndrcv_kafka_multi.sh\]: Sleep to give rsyslog instances time to process data ...
#sleep 20 

echo \[sndrcv_kafka_multi.sh\]: Stopping sender instance [omkafka]
shutdown_when_empty
wait_shutdown

echo \[sndrcv_kafka_multi.sh\]: Stopping receiver instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka_multi.sh\]: delete kafka topics 
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk1' '22181'

# Do the final sequence check
seq_check 1 $TESTMESSAGESFULL -d

echo \[sndrcv_kafka.sh\]: stop kafka instances
. $srcdir/diag.sh stop-kafka '.dep_wrk1'
. $srcdir/diag.sh stop-kafka '.dep_wrk2'
. $srcdir/diag.sh stop-kafka '.dep_wrk3'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk1'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk2'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk3'

echo success
exit_test