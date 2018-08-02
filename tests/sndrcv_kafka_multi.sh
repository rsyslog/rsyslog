#!/bin/bash
# added 2017-05-08 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=10000

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

echo \[sndrcv_kafka_multi.sh\]: Starting sender instance [omkafka]
export RSYSLOG_DEBUGLOG="log"
. $srcdir/diag.sh init
startup sndrcv_kafka_multi_rcvr.conf 
. $srcdir/diag.sh wait-startup

echo \[sndrcv_kafka_multi.sh\]: Starting receiver instance [imkafka]
export RSYSLOG_DEBUGLOG="log2"
startup sndrcv_kafka_multi_sender.conf 2
. $srcdir/diag.sh wait-startup 2

# now inject the messages into instance 2. It will connect to instance 1, and that instance will record the data.
tcpflood -m$TESTMESSAGES -i1

echo \[sndrcv_kafka_multi.sh\]: Sleep to give rsyslog instances time to process data ...
sleep 20 

echo \[sndrcv_kafka_multi.sh\]: Stopping sender instance [omkafka]
shutdown_when_empty
wait_shutdown

echo \[sndrcv_kafka_multi.sh\]: Stopping receiver instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

# Do the final sequence check
seq_check 1 $TESTMESSAGES -d

echo \[sndrcv_kafka.sh\]: stop kafka instances
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk1' '22181'
. $srcdir/diag.sh stop-kafka '.dep_wrk1'
. $srcdir/diag.sh stop-kafka '.dep_wrk2'
. $srcdir/diag.sh stop-kafka '.dep_wrk3'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk1'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk2'
. $srcdir/diag.sh stop-zookeeper '.dep_wrk3'
