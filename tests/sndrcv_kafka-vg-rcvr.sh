#!/bin/bash
# added 2017-05-03 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
export TESTMESSAGES=100000
# enable the EXTRA_EXITCHECK only if really needed - otherwise spams the test log
# too much
export EXTRA_EXITCHECK=dumpkafkalogs
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh stop-zookeeper
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk' '22181'

echo Give Kafka some time to process topic create ...
sleep 5

echo Starting receiver instance [omkafka]
export RSYSLOG_DEBUGLOG="log"
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg sndrcv_kafka_rcvr.conf 
. $srcdir/diag.sh wait-startup

echo Starting sender instance [imkafka]
export RSYSLOG_DEBUGLOG="log2"
. $srcdir/diag.sh startup sndrcv_kafka_sender.conf 2
. $srcdir/diag.sh wait-startup 2

echo Inject messages into rsyslog sender instance  
. $srcdir/diag.sh tcpflood -m$TESTMESSAGES -i1

echo Sleep to give rsyslog instances time to process data ...
sleep 5

echo Stopping sender instance [imkafka]
. $srcdir/diag.sh shutdown-when-empty 2
. $srcdir/diag.sh wait-shutdown 2

echo Sleep to give rsyslog receiver time to receive data ...
sleep 5

echo Stopping receiver instance [omkafka]
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg

# Do the final sequence check
. $srcdir/diag.sh seq-check 1 $TESTMESSAGES -d

echo stop kafka instance
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk' '22181'
. $srcdir/diag.sh stop-kafka

# STOP ZOOKEEPER in any case
. $srcdir/diag.sh stop-zookeeper

echo success
. $srcdir/diag.sh exit
