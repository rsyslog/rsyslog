#!/bin/bash
# Test for impstats push support - basic compilation and load test
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "Solaris has limitations"

generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats" 
       interval="1" 
       format="prometheus" 
       log.file="'$RSYSLOG_OUT_LOG'"
       resetCounters="off"
       
       # Push config (will fail to connect but that is OK for this test)
       push.url="http://localhost:8428/api/v1/write"
       push.labels=["test=impstats-push", "instance=prototype"]
       push.timeout.ms="1000"
)

action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)
'
startup

# Wait for impstats to generate at least one stats interval
# (interval is 1 second, wait 2 seconds to ensure stats are emitted)
sleep 2

shutdown_when_empty
wait_shutdown

# Verify stats were generated locally (push will fail but that's OK - no endpoint running)
content_check "resource-usage" "$RSYSLOG_OUT_LOG"
content_check "origin" "$RSYSLOG_OUT_LOG"

exit_test
