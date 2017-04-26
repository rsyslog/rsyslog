#!/bin/bash
# added 2017-05-08 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[sndrcv_kafka_multi.sh\]: Create multiple kafka/zookeeper instances and static topic
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh start-zookeeper
. $srcdir/diag.sh start-kafka '.dep_wrk1'
. $srcdir/diag.sh start-kafka '.dep_wrk2'
. $srcdir/diag.sh start-kafka '.dep_wrk3'
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk1' '2181'

echo \[sndrcv_kafka.sh\]: testing sending and receiving via kafka
. $srcdir/sndrcv_drvr.sh sndrcv_kafka_multi 10000

echo \[sndrcv_kafka.sh\]: stop kafka instances
. $srcdir/diag.sh delete-kafka-topic 'static' '.dep_wrk1' '2181'
. $srcdir/diag.sh stop-kafka '.dep_wrk1'
. $srcdir/diag.sh stop-kafka '.dep_wrk2'
. $srcdir/diag.sh stop-kafka '.dep_wrk3'
. $srcdir/diag.sh stop-zookeeper
