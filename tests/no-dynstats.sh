#!/bin/bash
# added 2016-03-10 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[no-dynstats.sh\]: test for verifying stats are reported correctly in legacy format in absence of any dynstats buckets being configured
. $srcdir/diag.sh init
startup no-dynstats.conf
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh custom-content-check 'global: origin=dynstats' 'rsyslog.out.stats.log'
exit_test
