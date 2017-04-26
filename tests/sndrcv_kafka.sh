#!/bin/bash
# added 2017-05-03 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[sndrcv_kafka.sh\]: Create kafka/zookeeper instance and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic 'static'

echo \[sndrcv_kafka.sh\]: testing sending and receiving via kafka
. $srcdir/sndrcv_drvr.sh sndrcv_kafka 1000

echo \[sndrcv_kafka.sh\]: stop kafka instance
. $srcdir/diag.sh delete-kafka-topic 'static'
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh stop-zookeeper

