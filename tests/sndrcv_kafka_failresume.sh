#!/bin/bash
# added 2017-06-06 by alorbach
#	This tests the keepFailedMessages feature in omkafka
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=1000
export TESTMESSAGESFULL=1000

echo ===============================================================================
echo \[sndrcv_kafka_failresume.sh\]: Create kafka/zookeeper instance and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk' '22181'

echo \[sndrcv_kafka_failresume.sh\]: Give Kafka some time to process topic create ...
sleep 5

echo \[sndrcv_kafka_failresume.sh\]: Init Testbench 
. $srcdir/diag.sh init

echo \[sndrcv_kafka_failresume.sh\]: Starting receiver instance [omkafka]
export RSYSLOG_DEBUGLOG="log"
startup sndrcv_kafka_rcvr.conf 
. $srcdir/diag.sh wait-startup

echo \[sndrcv_kafka_failresume.sh\]: Starting sender instance [imkafka]
export RSYSLOG_DEBUGLOG="log2"
startup sndrcv_kafka_sender.conf 2
. $srcdir/diag.sh wait-startup 2

echo \[sndrcv_kafka_failresume.sh\]: Inject messages into rsyslog sender instance  
. $srcdir/diag.sh tcpflood -m$TESTMESSAGES -i1

echo \[sndrcv_kafka_failresume.sh\]: Stopping kafka cluster instance 
. $srcdir/diag.sh stop-kafka

echo \[sndrcv_kafka_failresume.sh\]: Stopping sender instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka_failresume.sh\]: Starting kafka cluster instance 
. $srcdir/diag.sh start-kafka

echo \[sndrcv_kafka_failresume.sh\]: Starting sender instance [imkafka]
export RSYSLOG_DEBUGLOG="log3"
startup sndrcv_kafka_sender.conf 2
. $srcdir/diag.sh wait-startup 2

echo \[sndrcv_kafka_failresume.sh\]: Sleep to give rsyslog sender time to send data ...
sleep 10

echo \[sndrcv_kafka_failresume.sh\]: Stopping sender instance [imkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka_failresume.sh\]: Sleep to give rsyslog receiver time to receive data ...
sleep 5

echo \[sndrcv_kafka_failresume.sh\]: Stopping receiver instance [omkafka]
shutdown_when_empty
wait_shutdown

# Do the final sequence check
seq_check 1 $TESTMESSAGESFULL

echo \[sndrcv_kafka_failresume.sh\]: stop kafka instance
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk' '22181'
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper
