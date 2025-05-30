#!/bin/bash
# added 2016-03-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[stats-cee.sh\]: test for verifying stats are reported correctly cee format
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="1" severity="7" resetCounters="on" Ruleset="stats" bracketing="on" format="cee")

if ($msg == "this condition will never match") then {
  action(name="an_action_that_is_never_called" type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
injectmsg_file $srcdir/testsuites/dynstats_input_1
wait_queueempty
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
custom_content_check '@cee: { "name": "an_action_that_is_never_called", "origin": "core.action", "processed": 0, "failed": 0, "suspended": 0, "suspended.duration": 0, "resumed": 0 }' "${RSYSLOG_DYNNAME}.out.stats.log"
exit_test
