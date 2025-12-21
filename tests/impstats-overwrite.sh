#!/bin/bash
# test if log.file.overwrite works for impstats
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats"
	log.file=`echo $RSYSLOG_OUT_LOG`
	log.file.overwrite="on"
	interval="1")
'
startup
# Wait for at least two emissions
./msleep 2500
shutdown_when_empty
wait_shutdown

# Check how many times "resource-usage" appears in the log file.
# It should appear exactly once if overwrite is working correctly.
NUM_EMISSIONS=$(grep -c "resource-usage" $RSYSLOG_OUT_LOG)
cat -n $RSYSLOG_OUT_LOG

echo "Number of resource-usage entries found: $NUM_EMISSIONS"

if [ "$NUM_EMISSIONS" -ne 1 ]; then
    echo "FAIL: expected 1 emission in log file, but found $NUM_EMISSIONS"
    exit 1
fi

echo "SUCCESS: log.file.overwrite worked as expected"
exit_test
