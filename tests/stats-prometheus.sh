#!/bin/bash
# added 2025-07-12 by Codex
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="1" severity="7" resetCounters="on" Ruleset="stats" bracketing="on" format="prometheus")

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
# The main queue's legacy name contains a space. Prometheus output must use
# the reversible U__ form, including the doubled separator underscore.
custom_content_check '# TYPE U__main_20_Q__enqueued__total counter' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_assert_content_missing '@cee' "${RSYSLOG_DYNNAME}.out.stats.log"
exit_test