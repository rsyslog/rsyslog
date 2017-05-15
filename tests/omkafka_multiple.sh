#!/bin/bash
# added 2017-03-17 by Andre Lorbach
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh download-kafka
#. $srcdir/diag.sh start-kafka '.dep_wrk1'
. $srcdir/diag.sh start-kafka '.dep_wrk2'
. $srcdir/diag.sh start-kafka '.dep_wrk3'
#. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk1' '2181'
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk2' '2182'
. $srcdir/diag.sh create-kafka-topic 'static' '.dep_wrk3' '2183'

. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/omkafka/.libs/omkafka")

ruleset(name="stats") {
  action(type="omfile" file="./rsyslog.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="1" severity="7" resetCounters="on" Ruleset="stats" bracketing="on")

template(name="outfmt" type="string" string="%msg%\n")

if ($msg contains "msgnum") then {
	 action(name="kafka-fwd" type="omkafka" topic="static" broker=["localhost:9092", "localhost:9093", "localhost:9094"] template="outfmt" partitions.auto="on")
#	 action(name="kafka-fwd" type="omkafka" topic="static" broker="localhost:9092" template="outfmt" partitions.auto="on")
#	 action(name="kafka-fwd" type="omkafka" topic="static" broker="localhost:9093" template="outfmt" partitions.auto="on")
#	 action(name="kafka-fwd" type="omkafka" topic="static" broker="localhost:9094" template="outfmt" partitions.auto="on")
}
'
. $srcdir/diag.sh startup omkafka_multiple.conf
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh injectmsg  0 1000 #100000
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
#. $srcdir/diag.sh dump-kafka-topic 'static' '.dep_wrk1' '2181'
. $srcdir/diag.sh dump-kafka-topic 'static' '.dep_wrk2' '2182'
. $srcdir/diag.sh dump-kafka-topic 'static' '.dep_wrk3' '2183'
#. $srcdir/diag.sh stop-kafka '.dep_wrk1'
. $srcdir/diag.sh stop-kafka '.dep_wrk2'
. $srcdir/diag.sh stop-kafka '.dep_wrk3'
. $srcdir/diag.sh custom-content-check 'msgnum:00000000' 'rsyslog.out.kafka.dep_wrk1.log'
. $srcdir/diag.sh custom-content-check 'msgnum:00000001' 'rsyslog.out.kafka.dep_wrk2.log'
. $srcdir/diag.sh custom-content-check 'msgnum:00000009' 'rsyslog.out.kafka.dep_wrk3.log'
. $srcdir/diag.sh first-column-sum-check 's/.*submitted=\([0-9]\+\)/\1/g' 'omkafka' 'rsyslog.out.stats.log' 300000
. $srcdir/diag.sh exit
