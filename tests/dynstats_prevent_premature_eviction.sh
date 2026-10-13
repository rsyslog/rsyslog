#!/bin/bash
# A metric reused in each collection cycle must retain its accumulator even
# when unusedMetricLife (1 second) is shorter than the reporting interval (4).
# Park the stats reader before injecting each group, drain that group, then
# release exactly one complete BEGIN/END batch. This exercises survivor-table
# reuse without relying on scheduling or sleeps. The exact sums below prove
# that each group appeared in the intended cycle, not merely eventually.
# added 2016-04-13 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="4" severity="7" resetCounters="on" Ruleset="stats" bracketing="on")

template(name="outfmt" type="string" string="%msg% %$.increment_successful%\n")

dyn_stats(name="msg_stats" unusedMetricLife="1" resettable="off")

set $.msg_prefix = field($msg, 32, 1);

if (re_match($.msg_prefix, "foo|bar|baz|quux|corge|grault")) then {
  set $.increment_successful = dyn_inc("msg_stats", $.msg_prefix);
} else {
  set $.increment_successful = -1;
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
# The imdiag release acknowledgement precedes dynstats collection. END is
# emitted after every object's read notifier, including survivor-table rotation.
# Count completed batches while the reader is parked so the checkpoint cannot
# include an unrelated in-flight cycle.
collect_dynstats_cycle() {
    local completed
    completed=$(grep -c 'END$' "${RSYSLOG_DYNNAME}.out.stats.log" || true)
    . "$srcdir/diag.sh" allow-single-stats-flush-after-block-and-wait-for-it
    while [ "$(grep -c 'END$' "${RSYSLOG_DYNNAME}.out.stats.log" || true)" -le "$completed" ]; do
        if [ "$(date +%s)" -ge "$((TB_STARTTEST + TB_TEST_MAX_RUNTIME))" ]; then
            echo "Timeout waiting for a complete dynstats collection"
            error_exit 1
        fi
        rst_msleep 20
    done
}

startup
. "$srcdir/diag.sh" block-stats-flush
# This waits for the actual parked callback, including any initial collection
# that had already passed the gate when the block request was acknowledged.
echo "awaitStatsReportingBlocked" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT" || error_exit $?
for group in 1 2 3; do
    injectmsg_file "$srcdir/testsuites/dynstats_input_$group"
    wait_queueempty
    collect_dynstats_cycle
    # BEGIN is queued before the imdiag gate. Wait until the reader reaches
    # that gate again so it cannot finish a collection before the next input.
    echo "awaitStatsReportingBlocked" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT" || error_exit $?
done
# Release the final empty cycle before shutdown so impstats cannot remain
# parked during input termination. All tested accumulators are now survivors.
. "$srcdir/diag.sh" await-stats-flush-after-block
wait_queueempty
content_check "foo 001 0"
content_check "foo 006 0"
shutdown_when_empty
wait_shutdown
# Reusing the metrics each cycle must preserve their accumulated values.
custom_content_check 'baz=2' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'bar=1' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'foo=3' "${RSYSLOG_DYNNAME}.out.stats.log"
# Non-resettable values must appear in exactly the three controlled cycles.
first_column_sum_check 's/.*foo=\([0-9]*\)/\1/g' 'foo=' "${RSYSLOG_DYNNAME}.out.stats.log" 6
first_column_sum_check 's/.*bar=\([0-9]*\)/\1/g' 'bar=' "${RSYSLOG_DYNNAME}.out.stats.log" 1
first_column_sum_check 's/.*baz=\([0-9]*\)/\1/g' 'baz=' "${RSYSLOG_DYNNAME}.out.stats.log" 3
first_column_sum_check 's/.*new_metric_add=\([0-9]*\)/\1/g' 'new_metric_add=' "${RSYSLOG_DYNNAME}.out.stats.log" 3
first_column_sum_check 's/.*ops_overflow=\([0-9]*\)/\1/g' 'ops_overflow=' "${RSYSLOG_DYNNAME}.out.stats.log" 0
first_column_sum_check 's/.*no_metric=\([0-9]*\)/\1/g' 'no_metric=' "${RSYSLOG_DYNNAME}.out.stats.log" 0
exit_test
