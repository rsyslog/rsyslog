#!/bin/bash
# added 2015-11-13 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[dynstats_overflow-vg.sh\]: test for gathering stats when metrics exceed provisioned capacity
. $srcdir/diag.sh init
generate_conf
add_conf '
ruleset(name="stats") {
  action(type="omfile" file="./rsyslog.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="2" severity="7" resetCounters="on" Ruleset="stats" bracketing="on")

template(name="outfmt" type="string" string="%msg% %$.increment_successful%\n")

dyn_stats(name="msg_stats" unusedMetricLife="1" maxCardinality="3")

set $.msg_prefix = field($msg, 32, 1);

if (re_match($.msg_prefix, "foo|bar|baz|quux|corge|grault")) then {
  set $.increment_successful = dyn_inc("msg_stats", $.msg_prefix);
} else {
  set $.increment_successful = -1;
}

action(type="omfile" file="./rsyslog.out.log" template="outfmt")
'
startup_vg
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh block-stats-flush
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_more_0
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_more_1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it

. $srcdir/diag.sh first-column-sum-check 's/.*foo=\([0-9]\+\)/\1/g' 'foo=' 'rsyslog.out.stats.log' 5
. $srcdir/diag.sh first-column-sum-check 's/.*bar=\([0-9]\+\)/\1/g' 'bar=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*baz=\([0-9]\+\)/\1/g' 'baz=' 'rsyslog.out.stats.log' 2

. $srcdir/diag.sh custom-assert-content-missing 'quux' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'corge' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'grault' 'rsyslog.out.stats.log'

. $srcdir/diag.sh first-column-sum-check 's/.*new_metric_add=\([0-9]\+\)/\1/g' 'new_metric_add=' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh first-column-sum-check 's/.*ops_overflow=\([0-9]\+\)/\1/g' 'ops_overflow=' 'rsyslog.out.stats.log' 5
. $srcdir/diag.sh first-column-sum-check 's/.*no_metric=\([0-9]\+\)/\1/g' 'no_metric=' 'rsyslog.out.stats.log' 0

#ttl-expiry(2*ttl in worst case, ttl + delta in best) so metric-names reset should have happened
. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it
. $srcdir/diag.sh await-stats-flush-after-block

. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'

. $srcdir/diag.sh first-column-sum-check 's/.*metrics_purged=\([0-9]\+\)/\1/g' 'metrics_purged=' 'rsyslog.out.stats.log' 3

rm rsyslog.out.stats.log
. $srcdir/diag.sh issue-HUP #reopen stats file
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh block-stats-flush
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_more_2
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it

. $srcdir/diag.sh content-check "foo 001 0"
. $srcdir/diag.sh content-check "bar 002 0"
. $srcdir/diag.sh content-check "baz 003 0"
. $srcdir/diag.sh content-check "foo 004 0"
. $srcdir/diag.sh content-check "baz 005 0"
. $srcdir/diag.sh content-check "foo 006 0"
. $srcdir/diag.sh content-check "quux 007 -6"
. $srcdir/diag.sh content-check "corge 008 -6"
. $srcdir/diag.sh content-check "quux 009 -6"
. $srcdir/diag.sh content-check "foo 010 0"
. $srcdir/diag.sh content-check "corge 011 -6"
. $srcdir/diag.sh content-check "grault 012 -6"
. $srcdir/diag.sh content-check "foo 013 0"
. $srcdir/diag.sh content-check "corge 014 0"
. $srcdir/diag.sh content-check "grault 015 0"
. $srcdir/diag.sh content-check "quux 016 0"
. $srcdir/diag.sh content-check "foo 017 -6"
. $srcdir/diag.sh content-check "corge 018 0"

. $srcdir/diag.sh first-column-sum-check 's/.*corge=\([0-9]\+\)/\1/g' 'corge=' 'rsyslog.out.stats.log' 2
. $srcdir/diag.sh first-column-sum-check 's/.*grault=\([0-9]\+\)/\1/g' 'grault=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*quux=\([0-9]\+\)/\1/g' 'quux=' 'rsyslog.out.stats.log' 1

. $srcdir/diag.sh first-column-sum-check 's/.*new_metric_add=\([0-9]\+\)/\1/g' 'new_metric_add=' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh first-column-sum-check 's/.*ops_overflow=\([0-9]\+\)/\1/g' 'ops_overflow=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*no_metric=\([0-9]\+\)/\1/g' 'no_metric=' 'rsyslog.out.stats.log' 0

. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it
. $srcdir/diag.sh await-stats-flush-after-block

echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg

. $srcdir/diag.sh first-column-sum-check 's/.*metrics_purged=\([0-9]\+\)/\1/g' 'metrics_purged=' 'rsyslog.out.stats.log' 3

. $srcdir/diag.sh custom-assert-content-missing 'foo' 'rsyslog.out.stats.log'
exit_test
