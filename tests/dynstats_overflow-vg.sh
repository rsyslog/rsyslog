#!/bin/bash
# added 2015-11-13 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[dynstats_overflow-vg.sh\]: test for gathering stats when metrics exceed provisioned capacity
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg dynstats_overflow.conf
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_more_0
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh msleep 800
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_more_1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh msleep 1300 #sleep above + this = 2 seconds, so metric-names reset should have happened
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh first-column-sum-check 's/.*foo=\([0-9]\+\)/\1/g' 'foo=' 'rsyslog.out.stats.log' 5
. $srcdir/diag.sh first-column-sum-check 's/.*bar=\([0-9]\+\)/\1/g' 'bar=' 'rsyslog.out.stats.log' 1
. $srcdir/diag.sh first-column-sum-check 's/.*baz=\([0-9]\+\)/\1/g' 'baz=' 'rsyslog.out.stats.log' 2
. $srcdir/diag.sh custom-assert-content-missing 'quux' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'corge' 'rsyslog.out.stats.log'
. $srcdir/diag.sh custom-assert-content-missing 'grault' 'rsyslog.out.stats.log'
rm $srcdir/rsyslog.out.stats.log
. $srcdir/diag.sh issue-HUP #reopen stats file
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_more_2
. $srcdir/diag.sh msleep 2100
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
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

. $srcdir/diag.sh custom-assert-content-missing 'foo' 'rsyslog.out.stats.log'
. $srcdir/diag.sh exit
