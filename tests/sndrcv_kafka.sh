#!/bin/bash
# added 2017-05-03 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=1000
export TESTMESSAGESFULL=1000
# enable the EXTRA_EXITCHECK only if really needed - otherwise spams the test log
# too much
#export EXTRA_EXITCHECK=dumpkafkalogs
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

echo \[sndrcv_kafka.sh\]: Starting receiver instance [imkafka]
export RSYSLOG_DEBUGLOG="log"
. $srcdir/diag.sh init
startup sndrcv_kafka_rcvr.conf 
. $srcdir/diag.sh wait-startup

echo \[sndrcv_kafka.sh\]: Starting sender instance [omkafka]
export RSYSLOG_DEBUGLOG="log2"
startup sndrcv_kafka_sender.conf 2
. $srcdir/diag.sh wait-startup 2

echo \[sndrcv_kafka.sh\]: Inject messages into rsyslog sender instance  
. $srcdir/diag.sh tcpflood -m$TESTMESSAGES -i1

echo \[sndrcv_kafka.sh\]: Sleep to give rsyslog instances time to process data ...
sleep 5

echo \[sndrcv_kafka.sh\]: Stopping sender instance [omkafka]
shutdown_when_empty 2
wait_shutdown 2

echo \[sndrcv_kafka.sh\]: Sleep to give rsyslog receiver time to receive data ...
sleep 5

echo \[sndrcv_kafka.sh\]: Stopping receiver instance [imkafka]
shutdown_when_empty
wait_shutdown

# Do the final sequence check
seq_check 1 $TESTMESSAGESFULL -d

echo \[sndrcv_kafka_fail.sh\]: stop kafka instance
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk' '22181'
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper

echo success
exit_test
