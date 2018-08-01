#!/bin/bash
# test if HUP works for impstats
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats"
	log.file=`echo $RSYSLOG_OUT_LOG`
	interval="1" ruleset="stats")

ruleset(name="stats") {
	stop # nothing to do here
}
'
startup
./msleep 2000
mv  $RSYSLOG_OUT_LOG rsyslog2.out.log
. $srcdir/diag.sh issue-HUP
./msleep 2000
shutdown_when_empty
wait_shutdown
echo checking pre-HUP file
. $srcdir/diag.sh content-check 'global: origin=dynstats' rsyslog2.out.log
echo checking post-HUP file
. $srcdir/diag.sh content-check 'global: origin=dynstats' $RSYSLOG_OUT_LOG
exit_test
