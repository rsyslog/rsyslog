#!/bin/bash
# Test for impstats push support - basic compilation and load test
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "Solaris has limitations"

# Use external VM if provided, otherwise expect push to fail
if [ -n "$RSYSLOG_TESTBENCH_EXTERNAL_VM_URL" ]; then
	VM_URL="${RSYSLOG_TESTBENCH_EXTERNAL_VM_URL}/api/v1/write"
	echo "Using external VictoriaMetrics at: $VM_URL"
else
	VM_URL="http://localhost:19999/api/v1/write"
	echo "No external VM configured, push will fail (expected)"
	echo "Tip: for a local VM+Grafana stack, see devtools/vm_grafana_stack/README.md"
fi

generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats" 
       interval="1" 
       format="prometheus" 
       log.file="'$RSYSLOG_OUT_LOG'"
       resetCounters="off"
       
       # Push config
       push.url="'$VM_URL'"
       push.labels=["test=impstats-push", "instance=ci-test"]
       push.timeout.ms="2000"
       push.label.instance="on"
       push.label.job="rsyslog-test"
       push.label.origin="on"
       push.label.name="on"
)

action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)
'
startup

# Wait for impstats to generate and push at least 2-3 stats intervals
# (interval is 1 second, so wait 3 seconds to ensure push happens)
sleep 3

shutdown_when_empty
wait_shutdown

# Verify stats were generated locally
content_check "resource-usage" "$RSYSLOG_OUT_LOG"
content_check "origin" "$RSYSLOG_OUT_LOG"

exit_test
