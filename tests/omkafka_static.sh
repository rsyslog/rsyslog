#!/bin/bash
# added 2016-04-19 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[omkafka_static.sh\]: test for omkafka publishing to statically defined topic
. $srcdir/diag.sh init
. $srcdir/diag.sh download-kafka
. $srcdir/diag.sh start-kafka
. $srcdir/diag.sh create-kafka-topic 'static'
. $srcdir/diag.sh startup omkafka_static.conf
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh injectmsg  0 10
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh dump-kafka-topic 'static'
. $srcdir/diag.sh stop-kafka
. $srcdir/diag.sh custom-content-check 'msgnum:00000000' 'rsyslog.out.kafka.log'
. $srcdir/diag.sh custom-content-check 'msgnum:00000001' 'rsyslog.out.kafka.log'
. $srcdir/diag.sh custom-content-check 'msgnum:00000009' 'rsyslog.out.kafka.log'
. $srcdir/diag.sh first-column-sum-check 's/.*submitted=\([0-9]\+\)/\1/g' 'omkafka' 'rsyslog.out.stats.log' 10
. $srcdir/diag.sh exit
