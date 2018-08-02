#!/bin/bash
# added 2017-05-18 by alorbach
#	This test only tests what happens when kafka cluster fails
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=1000
export TESTMESSAGESFULL=2000

echo ===============================================================================
echo \[sndrcv_kafka_fail.sh\]: Create kafka/zookeeper instance and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk' '22181'

echo \[sndrcv_kafka_fail.sh\]: Give Kafka some time to process topic create ...
sleep 5

echo \[sndrcv_kafka_fail.sh\]: Stopping kafka cluster instance 
. $srcdir/diag.sh stop-kafka

echo \[sndrcv_kafka_fail.sh\]: Starting receiver instance [omkafka]
export RSYSLOG_DEBUGLOG="log"
. $srcdir/diag.sh init
startup sndrcv_kafka_rcvr.conf 
. $srcdir/diag.sh wait-startup

echo \[sndrcv_kafka_fail.sh\]: Starting sender instance [imkafka]
export RSYSLOG_DEBUGLOG="log2"
startup sndrcv_kafka_sender.conf 2
. $srcdir/diag.sh wait-startup 2

echo \[sndrcv_kafka_fail.sh\]: Inject messages into rsyslog sender instance  
tcpflood -m$TESTMESSAGES -i1

echo \[sndrcv_kafka_fail.sh\]: Starting kafka cluster instance 
. $srcdir/diag.sh start-kafka

echo \[sndrcv_kafka_fail.sh\]: Sleep to give rsyslog instances time to process data ...
sleep 5

echo \[sndrcv_kafka_fail.sh\]: Inject messages into rsyslog sender instance  
tcpflood -m$TESTMESSAGES -i1001

echo \[sndrcv_kafka_fail.sh\]: Sleep to give rsyslog sender time to send data ...
sleep 5

echo \[sndrcv_kafka_fail.sh\]: Stopping sender instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka_fail.sh\]: Sleep to give rsyslog receiver time to receive data ...
sleep 5

echo \[sndrcv_kafka_fail.sh\]: Stopping receiver instance [omkafka]
shutdown_when_empty
wait_shutdown

# Do the final sequence check
seq_check 1 $TESTMESSAGESFULL -d

echo \[sndrcv_kafka_fail.sh\]: stop kafka instance
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk' '22181'
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper
