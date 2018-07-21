#!/bin/bash
# added 2016-03-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[dynstats-json.sh\]: test for verifying stats are reported correctly in json format
. $srcdir/diag.sh init
startup dynstats-json.conf
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh custom-content-check '{ "name": "global", "origin": "dynstats", "values": { "stats_one.ops_overflow": 0, "stats_one.new_metric_add": 1, "stats_one.no_metric": 0, "stats_one.metrics_purged": 0, "stats_one.ops_ignored": 0, "stats_one.purge_triggered": 0, "stats_two.ops_overflow": 0, "stats_two.new_metric_add": 1, "stats_two.no_metric": 0, "stats_two.metrics_purged": 0, "stats_two.ops_ignored": 0, "stats_two.purge_triggered": 0 } }' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-content-check '{ "name": "stats_one", "origin": "dynstats.bucket", "values": { "foo": 1 } }' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-content-check '{ "name": "stats_two", "origin": "dynstats.bucket", "values": { "foo": 1 } }' 'rsyslog.out.stats.log'
exit_test
