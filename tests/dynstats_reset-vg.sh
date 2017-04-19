#!/bin/bash
# added 2015-11-13 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[dynstats_reset-vg.sh\]: test for gathering stats with a known-dyn-metrics reset in-between
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg dynstats_reset.conf
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_1
. $srcdir/diag.sh msleep 4100 #two seconds for unused-metrics to be kept under observation, another two them to be cleared off
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_2
. $srcdir/diag.sh msleep 4100
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_3
. $srcdir/diag.sh msleep 4100
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "foo 001 0"
. $srcdir/diag.sh content-check "bar 002 0"
. $srcdir/diag.sh content-check "baz 003 0"
. $srcdir/diag.sh content-check "foo 004 0"
. $srcdir/diag.sh content-check "baz 005 0"
. $srcdir/diag.sh content-check "foo 006 0"
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
 # because dyn-metrics would be reset before it can accumulate and report high counts, sleep between msg-injection ensures that
. $srcdir/diag.sh custom-assert-content-missing 'baz=2' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'foo=2' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'foo=3' 'rsyslog.out.stats.log'
# but actual reported stats (aggregate) should match
. $srcdir/diag.sh first-column-sum-check 's/.*foo=\([0-9]\+\)/\1/g' 'foo=' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh first-column-sum-check 's/.*bar=\([0-9]\+\)/\1/g' 'bar=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*baz=\([0-9]\+\)/\1/g' 'baz=' 'rsyslog.out.stats.log' 2
. $srcdir/diag.sh exit
