#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

if [ ! -r /proc/$$/stat ]; then
	echo "This test requires Linux /proc process CPU counters"
	skip_test
fi

get_proc_cpu_ticks() {
	awk '{ print $14 + $15 }' "/proc/$1/stat"
}

generate_conf
add_conf '
module(load="../contrib/improg/.libs/improg")

template(name="outfmt" type="string" string="%msg%\n")

input(type="improg" tag="tag" ruleset="improg_ruleset"
      binary="'${srcdir:=.}'/testsuites/improg-child-kill-bin.sh '$RSYSLOG_DYNNAME'.child.pid"
      confirmmessages="off" signalonclose="off" killunresponsive="on"
      closetimeout="1000"
     )

ruleset(name="improg_ruleset") {
    action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}

syslog.* {
    action(type="omfile" template="outfmt" file="'$RSYSLOG2_OUT_LOG'")
}
'

startup
wait_content "program data" "$RSYSLOG_OUT_LOG"
wait_file_lines "$RSYSLOG_DYNNAME.child.pid" 1 5

child_pid=$(cat "$RSYSLOG_DYNNAME.child.pid")
rsyslog_pid=$(getpid)
kill -s KILL "$child_pid"

ticks_before=$(get_proc_cpu_ticks "$rsyslog_pid")
$TESTTOOL_DIR/msleep 1200
ticks_after=$(get_proc_cpu_ticks "$rsyslog_pid")
ticks_delta=$((ticks_after - ticks_before))
echo "rsyslogd CPU ticks after improg child kill: $ticks_delta"
if [ "$ticks_delta" -gt 25 ]; then
	echo "improg appears to spin after child EOF"
	error_exit 1
fi

old_timeout="$TB_TEST_TIMEOUT"
TB_TEST_TIMEOUT=5
wait_content "(pid $child_pid) terminated by signal 9" "$RSYSLOG2_OUT_LOG"
TB_TEST_TIMEOUT="$old_timeout"

shutdown_when_empty
wait_shutdown

exit_test
