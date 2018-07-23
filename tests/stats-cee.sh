#!/bin/bash
# added 2016-03-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[stats-cee.sh\]: test for verifying stats are reported correctly cee format
. $srcdir/diag.sh init
startup stats-cee.conf
. $srcdir/diag.sh injectmsg-litteral $srcdir/testsuites/dynstats_input_1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh custom-content-check '@cee: { "name": "an_action_that_is_never_called", "origin": "core.action", "processed": 0, "failed": 0, "suspended": 0, "suspended.duration": 0, "resumed": 0 }' 'rsyslog.out.stats.log'
exit_test
