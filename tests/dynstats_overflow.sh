#!/bin/bash
# added 2015-11-13 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

wait_for_stats_metric() {
  local pattern="$1"
  local file="$2"
  local timeout_ms="${3:-30000}"
  local interval_ms=100
  local waited_ms=0

  while true; do
    if [ -f "$file" ] && grep -q "$pattern" "$file"; then
      return 0
    fi

    if [ $waited_ms -ge $timeout_ms ]; then
      echo "stats metric pattern '$pattern' not found in '$file' within ${timeout_ms}ms"
      if [ -f "$file" ]; then
        echo "file contents:"
        cat "$file"
      else
        echo "stats file '$file' does not exist"
      fi
      error_exit 1
    fi

    $TESTTOOL_DIR/msleep $interval_ms
    waited_ms=$((waited_ms + interval_ms))
  done
}
generate_conf
add_conf '
ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="2" severity="7" resetCounters="on" Ruleset="stats" bracketing="on")

template(name="outfmt" type="string" string="%msg% %$.increment_successful%\n")

dyn_stats(name="msg_stats" unusedMetricLife="1" maxCardinality="3")

set $.msg_prefix = field($msg, 32, 1);

if (re_match($.msg_prefix, "foo|bar|baz|quux|corge|grault")) then {
  set $.increment_successful = dyn_inc("msg_stats", $.msg_prefix);
} else {
  set $.increment_successful = -1;
}

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log
. $srcdir/diag.sh block-stats-flush
injectmsg_file $srcdir/testsuites/dynstats_input_more_0
injectmsg_file $srcdir/testsuites/dynstats_input_more_1
wait_queueempty
. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it

wait_for_stats_metric 'foo=' "${RSYSLOG_DYNNAME}.out.stats.log"
wait_for_stats_metric 'bar=' "${RSYSLOG_DYNNAME}.out.stats.log"
wait_for_stats_metric 'baz=' "${RSYSLOG_DYNNAME}.out.stats.log"

#first_column_sum_check 's/.*foo=\([0-9]*\)/\1/g' 'foo=' "${RSYSLOG_DYNNAME}.out.stats.log" 5
first_column_sum_check 's/.*foo=//g' 'foo=' "${RSYSLOG_DYNNAME}.out.stats.log" 5
first_column_sum_check 's/.*bar=//g' 'bar=' "${RSYSLOG_DYNNAME}.out.stats.log" 1
first_column_sum_check 's/.*baz=//g' 'baz=' "${RSYSLOG_DYNNAME}.out.stats.log" 2

custom_assert_content_missing 'quux' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_assert_content_missing 'corge' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_assert_content_missing 'grault' "${RSYSLOG_DYNNAME}.out.stats.log"

first_column_sum_check 's/.*new_metric_add=//g' 'new_metric_add=' "${RSYSLOG_DYNNAME}.out.stats.log" 3
first_column_sum_check 's/.*ops_overflow=//g' 'ops_overflow=' "${RSYSLOG_DYNNAME}.out.stats.log" 5
first_column_sum_check 's/.*no_metric=//g' 'no_metric=' "${RSYSLOG_DYNNAME}.out.stats.log" 0

#ttl-expiry(2*ttl in worst case, ttl + delta in best) so metric-names reset should have happened
. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it
. $srcdir/diag.sh await-stats-flush-after-block

wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log

wait_for_stats_metric 'metrics_purged=' "${RSYSLOG_DYNNAME}.out.stats.log"

first_column_sum_check 's/.*metrics_purged=//g' 'metrics_purged=' "${RSYSLOG_DYNNAME}.out.stats.log" 3

rm ${RSYSLOG_DYNNAME}.out.stats.log
issue_HUP #reopen stats file
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log
. $srcdir/diag.sh block-stats-flush
injectmsg_file $srcdir/testsuites/dynstats_input_more_2
wait_queueempty
. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it

wait_for_stats_metric 'corge=' "${RSYSLOG_DYNNAME}.out.stats.log"
wait_for_stats_metric 'grault=' "${RSYSLOG_DYNNAME}.out.stats.log"
wait_for_stats_metric 'quux=' "${RSYSLOG_DYNNAME}.out.stats.log"

content_check "foo 001 0"
content_check "bar 002 0"
content_check "baz 003 0"
content_check "foo 004 0"
content_check "baz 005 0"
content_check "foo 006 0"
content_check "quux 007 -6"
content_check "corge 008 -6"
content_check "quux 009 -6"
content_check "foo 010 0"
content_check "corge 011 -6"
content_check "grault 012 -6"
content_check "foo 013 0"
content_check "corge 014 0"
content_check "grault 015 0"
content_check "quux 016 0"
content_check "foo 017 -6"
content_check "corge 018 0"

first_column_sum_check 's/.*corge=//g' 'corge=' "${RSYSLOG_DYNNAME}.out.stats.log" 2
first_column_sum_check 's/.*grault=//g' 'grault=' "${RSYSLOG_DYNNAME}.out.stats.log" 1
first_column_sum_check 's/.*quux=//g' 'quux=' "${RSYSLOG_DYNNAME}.out.stats.log" 1

first_column_sum_check 's/.*new_metric_add=//g' 'new_metric_add=' "${RSYSLOG_DYNNAME}.out.stats.log" 3
first_column_sum_check 's/.*ops_overflow=//g' 'ops_overflow=' "${RSYSLOG_DYNNAME}.out.stats.log" 1
first_column_sum_check 's/.*no_metric=//g' 'no_metric=' "${RSYSLOG_DYNNAME}.out.stats.log" 0

. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it
. $srcdir/diag.sh await-stats-flush-after-block

echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown

first_column_sum_check 's/.*metrics_purged=//g' 'metrics_purged=' "${RSYSLOG_DYNNAME}.out.stats.log" 3

custom_assert_content_missing 'foo' "${RSYSLOG_DYNNAME}.out.stats.log"
exit_test
