#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify that the rsyslog-assigned action name, including its hyphens and
# colon, is encoded into a valid Prometheus metric name. The emitted stats file
# is the oracle: it must contain the exact values-style name and never the raw name.

. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="1" severity="7" resetCounters="on" Ruleset="stats" bracketing="on" format="prometheus")

action(type="omfile" file="'${RSYSLOG_OUT_LOG}'")
'

startup
injectmsg_file $srcdir/testsuites/dynstats_input_1
wait_queueempty
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log
shutdown_when_empty
wait_shutdown

custom_content_check 'U__action_2D_0_2D_builtin:omfile__processed__total' \
    "${RSYSLOG_DYNNAME}.out.stats.log"
custom_assert_content_missing 'action-0-builtin:omfile_processed_total' \
    "${RSYSLOG_DYNNAME}.out.stats.log"
exit_test