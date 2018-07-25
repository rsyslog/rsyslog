#!/bin/bash
# added 2016-03-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[dynstats-json-vg.sh\]: test for verifying stats are reported correctly in json format with valgrind
. $srcdir/diag.sh init
generate_conf
add_conf '
dyn_stats(name="stats_one")
dyn_stats(name="stats_two")

ruleset(name="stats") {
  action(type="omfile" file="./rsyslog.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="2" severity="7" resetCounters="on" Ruleset="stats" bracketing="on" format="json")

template(name="outfmt" type="string" string="%msg%\n")

set $.p = field($msg, 32, 1);
if ($.p == "foo") then {
  set $.ign = dyn_inc("stats_one", $.p);
  set $.ign = dyn_inc("stats_two", $.p);
}

action(type="omfile" file="./rsyslog.out.log" template="outfmt")
'
startup_vg
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh custom-content-check '{ "name": "global", "origin": "dynstats", "values": { "stats_one.ops_overflow": 0, "stats_one.new_metric_add": 1, "stats_one.no_metric": 0, "stats_one.metrics_purged": 0, "stats_one.ops_ignored": 0, "stats_one.purge_triggered": 0, "stats_two.ops_overflow": 0, "stats_two.new_metric_add": 1, "stats_two.no_metric": 0, "stats_two.metrics_purged": 0, "stats_two.ops_ignored": 0, "stats_two.purge_triggered": 0 } }' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-content-check '{ "name": "stats_one", "origin": "dynstats.bucket", "values": { "foo": 1 } }' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-content-check '{ "name": "stats_two", "origin": "dynstats.bucket", "values": { "foo": 1 } }' 'rsyslog.out.stats.log'
exit_test
