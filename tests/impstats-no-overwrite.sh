#!/bin/bash
# test if impstats appends by default (no log.file.overwrite)
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats"
	log.file=`echo $RSYSLOG_OUT_LOG`
	interval="1")
'
startup
# Wait for at least two emissions
./msleep 2500
shutdown_when_empty
wait_shutdown

# Check how many times "resource-usage" appears in the log file.
# It should appear at least twice.
NUM_EMISSIONS=$(grep -c "resource-usage" $RSYSLOG_OUT_LOG)

echo "Number of resource-usage entries found: $NUM_EMISSIONS"

if [ "$NUM_EMISSIONS" -lt 2 ]; then
    echo "FAIL: expected at least 2 emissions in log file, but found $NUM_EMISSIONS"
    exit 1
fi

echo "SUCCESS: impstats appended as expected"
exit_test
