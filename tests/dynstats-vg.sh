#!/bin/bash
# added 2015-11-13 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[dynstats-vg.sh\]: test for gathering stats over dynamic metric names with valgrind
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg dynstats.conf
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "foo 001 0"
. $srcdir/diag.sh content-check "bar 002 0"
. $srcdir/diag.sh content-check "baz 003 0"
. $srcdir/diag.sh content-check "foo 004 0"
. $srcdir/diag.sh content-check "baz 005 0"
. $srcdir/diag.sh content-check "foo 006 0"
sleep 2 # wait for stats flush
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh custom-content-check 'bar=1' 'rsyslog.out.stats.log'
. $srcdir/diag.sh first-column-sum-check 's/.*foo=\([0-9]\+\)/\1/g' 'rsyslog.out.stats.log' 3
. $srcdir/diag.sh first-column-sum-check 's/.*bar=\([0-9]\+\)/\1/g' 'rsyslog.out.stats.log' 2
. $srcdir/diag.sh exit
