#!/bin/bash
# added 2017-05-03 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=1000
export TESTMESSAGESFULL=1000
echo ===============================================================================
echo \[sndrcv_kafka.sh\]: Create kafka/zookeeper instance and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk' '22181'

echo \[sndrcv_kafka.sh\]: Give Kafka some time to process topic create ...
sleep 5

echo \[sndrcv_kafka.sh\]: Starting receiver instance [omkafka]
export RSYSLOG_DEBUGLOG="log"
. $srcdir/diag.sh init
. $srcdir/diag.sh startup sndrcv_kafka_rcvr.conf 
. $srcdir/diag.sh wait-startup

echo \[sndrcv_kafka.sh\]: Starting sender instance [imkafka]
export RSYSLOG_DEBUGLOG="log2"
. $srcdir/diag.sh startup sndrcv_kafka_sender.conf 2
. $srcdir/diag.sh wait-startup 2

echo \[sndrcv_kafka.sh\]: Inject messages into rsyslog sender instance  
. $srcdir/diag.sh tcpflood -m$TESTMESSAGES -i1

echo \[sndrcv_kafka.sh\]: Sleep to give rsyslog instances time to process data ...
sleep 5

echo \[sndrcv_kafka.sh\]: Stopping sender instance [imkafka]
. $srcdir/diag.sh shutdown-when-empty 2
. $srcdir/diag.sh wait-shutdown 2

echo \[sndrcv_kafka.sh\]: Sleep to give rsyslog receiver time to receive data ...
sleep 5

echo \[sndrcv_kafka.sh\]: Stopping receiver instance [omkafka]
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

# Do the final sequence check
. $srcdir/diag.sh seq-check 1 $TESTMESSAGESFULL -d

echo \[sndrcv_kafka_fail.sh\]: stop kafka instance
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk' '22181'
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper
