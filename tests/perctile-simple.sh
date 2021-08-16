#!/bin/bash

. ${srcdir:=.}/diag.sh init
DELIMITER='|'
BUCKETNAME='test_bucket'
STATNAME='test_stat_name'
# uncomment to test really long statname
#STATNAME='123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890'
generate_conf
add_conf '
ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="1" severity="7" Ruleset="stats" bracketing="on")
#module(load="../plugins/impstats/.libs/impstats" format="json" interval="1" severity="7" Ruleset="stats" bracketing="on")
template(name="outfmt" type="string" string="%$.timestamp% %msg%  val=%$.val%\n")

percentile_stats(name="'$BUCKETNAME'"
  percentiles=["95", "50", "99"]
  windowsize="1000"
  delimiter="'${DELIMITER}'"
  )

if $msg startswith " msgnum:" then {
  # test with a small window
  set $.val = field($msg, 58, 2);
  set $.status = percentile_observe("'$BUCKETNAME'", "'$STATNAME'", $.val);
  action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log
. $srcdir/diag.sh block-stats-flush
shuf -i 1-1000 | sed -e 's/^/injectmsg literal <167>Mar  1 01:00:00 172.20.245.8 tag msgnum:/g' | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
wait_queueempty
. $srcdir/diag.sh allow-single-stats-flush-after-block-and-wait-for-it

shuf -i 1001-2000 | sed -e 's/^/injectmsg literal <167>Mar  1 01:00:00 172.20.245.8 tag msgnum:/g' | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
. $srcdir/diag.sh await-stats-flush-after-block
wait_queueempty
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log

echo doing shutdown
shutdown_when_empty
custom_content_check "${STATNAME}${DELIMITER}p95=950" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}p50=500" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}p99=990" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_min=1" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_max=1000" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_sum=500500" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_count=1000" "${RSYSLOG_DYNNAME}.out.stats.log"

custom_content_check "${STATNAME}${DELIMITER}p95=1950" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}p50=1500" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}p99=1990" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_min=1001" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_max=2000" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_sum=1500500" "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check "${STATNAME}${DELIMITER}window_count=1000" "${RSYSLOG_DYNNAME}.out.stats.log"

exit_test
