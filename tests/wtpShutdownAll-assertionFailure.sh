#!/bin/bash
# add 2018-04-19 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
$AbortOnUncleanConfig on
$LocalHostName wtpshutdownall
$PreserveFQDN on

global(
	workDirectory="test-spool"
)

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/impstats/.libs/impstats" interval="300"
	resetCounters="on" format="cee" ruleset="metrics-impstat" log.syslog="on")

ruleset(name="metrics-impstat" queue.type="Direct"){
	action(type="omfile" file="test-spool/stats.log")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

# This test only checks that rsyslog does not abort
# so we don't need to check for output.

. $srcdir/diag.sh exit
