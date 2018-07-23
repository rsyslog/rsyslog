#!/bin/bash
# added 2015-11-13 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[dynstats_reset.sh\]: test for gathering stats with a known-dyn-metrics reset inbetween
. $srcdir/diag.sh init
startup dynstats_reset.conf
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_1
rst_msleep 8100
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_2
rst_msleep 8100
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_3
rst_msleep 8100
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "foo 001 0"
. $srcdir/diag.sh content-check "bar 002 0"
. $srcdir/diag.sh content-check "baz 003 0"
. $srcdir/diag.sh content-check "foo 004 0"
. $srcdir/diag.sh content-check "baz 005 0"
. $srcdir/diag.sh content-check "foo 006 0"
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
 # because dyn-metrics would be reset before it can accumulate and report high counts, sleep between msg-injection ensures that
. $srcdir/diag.sh custom-assert-content-missing 'baz=2' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'foo=2' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'foo=3' 'rsyslog.out.stats.log'
# but actual reported stats (aggregate) should match
. $srcdir/diag.sh first-column-sum-check 's/.*foo=\([0-9]\+\)/\1/g' 'foo=' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh first-column-sum-check 's/.*bar=\([0-9]\+\)/\1/g' 'bar=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*baz=\([0-9]\+\)/\1/g' 'baz=' 'rsyslog.out.stats.log' 2
. $srcdir/diag.sh first-column-sum-check 's/.*new_metric_add=\([0-9]\+\)/\1/g' 'new_metric_add=' 'rsyslog.out.stats.log' 6
. $srcdir/diag.sh first-column-sum-check 's/.*ops_overflow=\([0-9]\+\)/\1/g' 'ops_overflow=' 'rsyslog.out.stats.log' 0
. $srcdir/diag.sh first-column-sum-check 's/.*no_metric=\([0-9]\+\)/\1/g' 'no_metric=' 'rsyslog.out.stats.log' 0
. $srcdir/diag.sh first-column-sum-check 's/.*metrics_purged=\([0-9]\+\)/\1/g' 'metrics_purged=' 'rsyslog.out.stats.log' 6
exit_test
