#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#export RSYSLOG_DEBUG="Debug"
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")

ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}

module(load="../plugins/impstats/.libs/impstats" interval="1" severity="7" resetCounters="on" Ruleset="stats" bracketing="on")
template(name="outfmt" type="string" string="%msg% %$.increment_successful%\n")

dyn_stats(name="msg_stats" resettable="off" persistStateInterval="1" statefile.directory="'${RSYSLOG_DYNNAME}'.spool")

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
injectmsg_file $srcdir/testsuites/dynstats_input_more_0
wait_queueempty
rst_msleep 1100 # wait for stats flush
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown

custom_content_check 'foo=4' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'baz=2' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'bar=1' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'corge=1' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'quux=2' "${RSYSLOG_DYNNAME}.out.stats.log"

# check above counts stats have been persisted.
ls -l ${RSYSLOG_DYNNAME}.spool
echo restarting rsyslog
startup
echo restarted rsyslog, continuing with test
injectmsg_file $srcdir/testsuites/dynstats_input_more_2
wait_queueempty
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
custom_content_check 'foo=5' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'baz=2' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'bar=1' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'corge=3' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'quux=3' "${RSYSLOG_DYNNAME}.out.stats.log"
custom_content_check 'grault=1' "${RSYSLOG_DYNNAME}.out.stats.log"
exit_test
