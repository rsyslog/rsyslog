#!/bin/bash
# added 2015-11-16 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[dynstats_reset_without_pstats_reset.sh\]: test to ensure correctness of stats-ctr reset when pstats reset is turned off
. $srcdir/diag.sh init
startup dynstats_reset_without_pstats_reset.conf
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_1
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_2
. $srcdir/diag.sh wait-queueempty
sleep 1
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_3
. $srcdir/diag.sh wait-queueempty
sleep 1
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh content-check "foo 006"
. $srcdir/diag.sh custom-content-check 'foo=3' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-content-check 'bar=1' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-content-check 'baz=2' 'rsyslog.out.stats.log'
. $srcdir/diag.sh first-column-sum-check 's/.*foo=\([0-9]\+\)/\1/g' 'msg_stats_resettable_on.\+foo=' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh first-column-sum-check 's/.*bar=\([0-9]\+\)/\1/g' 'msg_stats_resettable_on.\+bar=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*baz=\([0-9]\+\)/\1/g' 'msg_stats_resettable_on.\+baz=' 'rsyslog.out.stats.log' 2
. $srcdir/diag.sh assert-first-column-sum-greater-than 's/.*foo=\([0-9]\+\)/\1/g' 'msg_stats_resettable_off.\+foo=' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh assert-first-column-sum-greater-than 's/.*bar=\([0-9]\+\)/\1/g' 'msg_stats_resettable_off.\+bar=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh assert-first-column-sum-greater-than 's/.*baz=\([0-9]\+\)/\1/g' 'msg_stats_resettable_off.\+baz=' 'rsyslog.out.stats.log' 2
. $srcdir/diag.sh first-column-sum-check 's/.*foo=\([0-9]\+\)/\1/g' 'msg_stats_resettable_default.\+foo=' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh first-column-sum-check 's/.*bar=\([0-9]\+\)/\1/g' 'msg_stats_resettable_default.\+bar=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*baz=\([0-9]\+\)/\1/g' 'msg_stats_resettable_default.\+baz=' 'rsyslog.out.stats.log' 2
exit_test
